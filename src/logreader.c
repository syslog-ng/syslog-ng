/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
  
#include "logreader.h"
#include "messages.h"
#include "logproto.h"
#include "misc.h"
#include "stats.h"
#include "tags.h"
#include "cfg-parser.h"
#include "compat.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * FIXME: LogReader has grown big enough that it is difficult to
 * maintain it. The root of the problem is a design issue, instead of
 * having each source driver derive from LogReader, they use a
 * log-reader object. As most of the common options are implemented
 * here, each driver specific hack was added to logreader, making it
 * much more difficult to read and modify.
 *
 * Examples: 
 * 
 *  - file position tracking, follow-freq and immediate-check are file
 *    source specific
 *  - name resolution and peer addresses applies only to network based sources
 *  - etc.
 *
 * The solution I have in mind is to get rid of LogDriver (which is
 * very thin anyway, basically is only a "next" pointer in the linked
 * list of an s/dgroup, then convert sgroup and dgroup to use GList or
 * GPtrArray, this way sgroup/dgroup would not rely on LogDriver
 * instances, it'd simple use LogPipes. Then, each of the source
 * drivers would be derived from LogReader (probably after a rename to
 * reflect the change), and each driver specific features would be
 * added by using virtual methods instead of using a monolithic class.
 *
 * Of course a similar change can be applied to LogWriters as well.
 **/


static gboolean log_reader_fetch_log(LogReader *self, LogProto *proto);

struct _LogReaderWatch
{
  GSource super;
  LogReader *reader;
  GPollFD pollfd;
  LogProto *proto;
  GTimeVal last_follow_freq_check;
};

static gboolean
log_reader_fd_prepare(GSource *source,
                      gint *timeout)
{
  LogReaderWatch *self = (LogReaderWatch *) source;
  GIOCondition proto_cond;

  self->pollfd.revents = 0;
  self->pollfd.events = G_IO_ERR;

  /* never indicate readability if flow control prevents us from sending messages */
  
  if (!log_source_free_to_send(&self->reader->super))
    return FALSE;

  if (log_proto_prepare(self->proto, &self->pollfd.fd, &proto_cond, timeout))
    return TRUE;

  if (self->reader->immediate_check)
    {
      *timeout = 0;
      self->reader->immediate_check = FALSE;
      return FALSE;
    }

  if (self->reader->options->follow_freq > 0)
    {
      *timeout = self->reader->options->follow_freq;
      return FALSE;
    }
  
  self->pollfd.events |= proto_cond;
  return FALSE;
}

static gboolean
log_reader_fd_check(GSource *source)
{
  LogReaderWatch *self = (LogReaderWatch *) source;
 
  if (!log_source_free_to_send(&self->reader->super))
    return FALSE;

  if (self->reader->options->follow_freq > 0)
    {
      struct stat st, followed_st;
      off_t pos = -1;
      gint fd = log_proto_get_fd(self->reader->proto);

      /* FIXME: this is a _HUGE_ layering violation... */
      if (fd >= 0)
	{
	  pos = lseek(fd, 0, SEEK_CUR);      
	  if (pos == (off_t) -1)
	    {
	      msg_error("Error invoking seek on followed file",
			evt_tag_errno("error", errno),
			NULL);
	      return FALSE;
	    }

	  if (fstat(fd, &st) < 0)
	    {
	      msg_error("Error invoking fstat() on followed file",
			evt_tag_errno("error", errno),
			NULL);
	      return FALSE;
	    }

          msg_trace("log_reader_fd_check",
              	    evt_tag_int("pos", pos),
                    evt_tag_int("size", st.st_size),
		    NULL);

	  if (pos < st.st_size)
            {
	      return TRUE;
            }
          else if (pos == st.st_size)
            {
              log_pipe_notify(self->reader->control, &self->reader->super.super, NC_FILE_EOF, self);
            }
          else if (pos > st.st_size)
            { 
              /* reopen the file */
              log_pipe_notify(self->reader->control, &self->reader->super.super, NC_FILE_MOVED, self);
            }
	} 
        
      if (self->reader->follow_filename && stat(self->reader->follow_filename, &followed_st) != -1)
        {
          if (fd < 0 || (st.st_ino != followed_st.st_ino && followed_st.st_size > 0))
            {
              msg_trace("log_reader_fd_check file moved eof",
                        evt_tag_int("pos", pos),
                        evt_tag_int("size", followed_st.st_size),
                        evt_tag_str("follow_filename",self->reader->follow_filename),
                        NULL);
              /* file was moved and we are at EOF, follow the new file */
              log_pipe_notify(self->reader->control, &self->reader->super.super, NC_FILE_MOVED, self);
            }
        }
      else if (self->reader->follow_filename)
        {
          GTimeVal now;

          g_source_get_current_time(source, &now);
          if (g_time_val_diff(&now, &self->last_follow_freq_check) > self->reader->options->follow_freq * 1000)
            {
              msg_verbose("Follow mode file still does not exist",
                          evt_tag_str("filename", self->reader->follow_filename),
                          NULL);
              self->last_follow_freq_check = now;
            }
        }
      return FALSE;
    }
    
  return !!(self->pollfd.revents & (G_IO_IN | G_IO_OUT | G_IO_ERR | G_IO_HUP));
}

static gboolean
log_reader_fd_dispatch(GSource *source,
                       GSourceFunc callback,
                       gpointer user_data)
{
  LogReaderWatch *self = (LogReaderWatch *) source;

  /* The window status can change between check() and dispatch()
   * because multiple tcp connections can have messages ready
   * from the same source at check() time, but the queue may
   * fill before we dispatch() them all
   */
  if (!log_source_free_to_send(&self->reader->super))
    return TRUE;

  if (!log_reader_fetch_log(self->reader, self->proto))
    {
      return FALSE;
    }
    
  return TRUE;
}

static void
log_reader_fd_finalize(GSource *source)
{
  LogReaderWatch *self = (LogReaderWatch *) source;
  log_proto_free(self->proto);
  log_pipe_unref(&self->reader->super.super);
}

GSourceFuncs log_reader_source_funcs =
{
  log_reader_fd_prepare,
  log_reader_fd_check,
  log_reader_fd_dispatch,
  log_reader_fd_finalize
};

static LogReaderWatch *
log_reader_watch_new(LogReader *reader, LogProto *proto)
{
  LogReaderWatch *self = (LogReaderWatch *) g_source_new(&log_reader_source_funcs, sizeof(LogReaderWatch));
  
  log_pipe_ref(&reader->super.super);
  self->reader = reader;
  self->proto = proto;
  g_source_set_priority(&self->super, LOG_PRIORITY_READER);
  g_source_add_poll(&self->super, &self->pollfd);
  return self;
}


static gboolean
log_reader_handle_line(LogReader *self, const guchar *line, gint length, GSockAddr *saddr, guint parse_flags)
{
  LogMessage *m;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  gint i;
  
  msg_debug("Incoming log entry", 
            evt_tag_printf("line", "%.*s", length, line),
            NULL);
  /* use the current time to get the time zone offset */
  m = log_msg_new((gchar *) line, length,
                  saddr,
                  &self->options->parse_options);
  
  if (!m->saddr && self->peer_addr)
    {
      m->saddr = g_sockaddr_ref(self->peer_addr);
    }

  if (self->options->tags)
    for (i = 0; i < self->options->tags->len; i++)
      log_msg_set_tag_by_id(m, g_array_index(self->options->tags, guint, i));

  log_msg_set_tag_by_id(m, self->super.options->source_group_tag);

  log_pipe_queue(&self->super.super, m, &path_options);
  return log_source_free_to_send(&self->super);
}

static gboolean
log_reader_fetch_log(LogReader *self, LogProto *proto)
{
  GSockAddr *sa;
  gint msg_count = 0;
  gboolean may_read = TRUE;
  guint parse_flags = 0;

  if (self->waiting_for_preemption)
    may_read = FALSE;
    
  /* NOTE: this loop is here to decrease the load on the main loop, we try
   * to fetch a couple of messages in a single run (but only up to
   * fetch_limit).
   */
  while (msg_count < self->options->fetch_limit)
    {
      const guchar *msg;
      gsize msg_len;
      LogProtoStatus status;

      msg = NULL;
      sa = NULL;

      /* NOTE: may_read is used to implement multi-read checking. It
       * is initialized to TRUE to indicate that the protocol is
       * allowed to issue a read(). If multi-read is disallowed in the
       * protocol, it resets may_read to FALSE after the first read was issued.
       */

      status = log_proto_fetch(self->proto, &msg, &msg_len, &sa, &may_read);
      switch (status)
        {
        case LPS_EOF:
        case LPS_ERROR:
          log_pipe_notify(self->control, &self->super.super, status == LPS_ERROR ? NC_READ_ERROR : NC_CLOSE, self);
          g_sockaddr_unref(sa);
          return FALSE;
        case LPS_SUCCESS:
          break;
        default:
          g_assert_not_reached();
          break;
        }

      if (!msg)
        {
          /* no more messages for now */
          break;
        }
      if (msg_len > 0 || (self->options->flags & LR_EMPTY_LINES))
        {
          msg_count++;

          if (!log_reader_handle_line(self, msg, msg_len, sa, parse_flags))
            {
              /* window is full, don't generate further messages */
              log_proto_queued(self->proto);
              g_sockaddr_unref(sa);
              break;
            }
        }
      log_proto_queued(self->proto);
      g_sockaddr_unref(sa);
    }
  if (self->options->flags & LR_PREEMPT)
    {
      /* NOTE: we assume that self->proto is a LogProtoFileReader */
      LogProtoFileReader *s = (LogProtoFileReader*)self->proto;
      
      if (log_proto_file_reader_is_preemptable(s))
        {
          self->waiting_for_preemption = FALSE;
          log_pipe_notify(self->control, &self->super.super, NC_FILE_SKIP, self);
        }
      else
        {
          self->waiting_for_preemption = TRUE;
        }
    }
  return TRUE;
}

static gboolean
log_reader_init(LogPipe *s)
{
  LogReader *self = (LogReader *) s;

  if (!log_source_init(s))
    return FALSE;
  /* check for new data */
  if (self->options->padding)
    {
      if (self->options->msg_size < self->options->padding)
	{
	  msg_error("Buffer is too small to hold padding number of bytes",
	            evt_tag_int("padding", self->options->padding),
                    evt_tag_int("msg_size", self->options->msg_size),
                    NULL);
	  return FALSE;
	}
    }
  if (self->options->text_encoding)
    {
      if (!log_proto_set_encoding(self->proto, self->options->text_encoding))
        {
          msg_error("Unknown character set name specified",
                    evt_tag_str("encoding", self->options->text_encoding),
                    NULL);
          return FALSE;
        }
      
    }
  /* the source added below references this logreader, it will be unref'd
     when the source is destroyed */ 
  self->source = log_reader_watch_new(self, self->proto);
  g_source_attach(&self->source->super, NULL);
    
  return TRUE;
}

static gboolean
log_reader_deinit(LogPipe *s)
{
  LogReader *self = (LogReader *) s;
  
  if (self->source)
    {
      g_source_destroy(&self->source->super);
      g_source_unref(&self->source->super);
      self->source = NULL;
    }
    
  if (!log_source_deinit(s))
    return FALSE;

  return TRUE;
}

static void
log_reader_free(LogPipe *s)
{
  LogReader *self = (LogReader *) s;
  
  /* when this function is called the source is already removed, because it
     holds a reference to this reader */
  log_pipe_unref(self->control);
  g_sockaddr_unref(self->peer_addr);
  g_free(self->follow_filename);
  log_source_free(s);
}

void
log_reader_set_options(LogPipe *s, LogPipe *control, LogReaderOptions *options, gint stats_level, gint stats_source, const gchar *stats_id, const gchar *stats_instance)
{
  LogReader *self = (LogReader *) s;

  log_source_set_options(&self->super, &options->super, stats_level, stats_source, stats_id, stats_instance);

  log_pipe_unref(self->control);
  log_pipe_ref(control);
  self->control = control;

  self->options = options;
}

void
log_reader_set_follow_filename(LogPipe *s, const gchar *follow_filename)
{
  /* try to free */
  LogReader *self = (LogReader *) s;
  g_free(self->follow_filename);
  self->follow_filename = g_strdup(follow_filename);
}

void
log_reader_set_peer_addr(LogPipe *s, GSockAddr *peer_addr)
{
  LogReader *self = (LogReader *) s;
  self->peer_addr = g_sockaddr_ref(peer_addr);
}

LogPipe *
log_reader_new(LogProto *proto)
{
  LogReader *self = g_new0(LogReader, 1);

  log_source_init_instance(&self->super);
  self->super.super.init = log_reader_init;
  self->super.super.deinit = log_reader_deinit;
  self->super.super.free_fn = log_reader_free;
  self->proto = proto;
  self->immediate_check = FALSE;
  return &self->super.super;
}

void 
log_reader_set_immediate_check(LogPipe *s)
{
  LogReader *self = (LogReader*) s;
  
  self->immediate_check = TRUE;
}

void
log_reader_options_defaults(LogReaderOptions *options)
{
  log_source_options_defaults(&options->super);
  log_parse_syslog_options_defaults(&options->parse_options);
  options->padding = 0;
  options->fetch_limit = -1;
  options->msg_size = -1;
  options->follow_freq = -1; 
  options->text_encoding = NULL;
  if (configuration && configuration->version < 0x0300)
    {
      static gboolean warned;
      if (!warned)
        {
          msg_warning("WARNING: input: sources do not remove new-line characters from messages by default in version 3.0, please add 'no-multi-line' flag to your configuration if you want to retain this functionality",
                      NULL);
          warned = TRUE;
        }
      options->parse_options.flags |= LP_NO_MULTI_LINE;
    }
}

/*
 * NOTE: options_init and options_destroy are a bit weird, because their
 * invocation is not completely symmetric:
 *
 *   - init is called from driver init (e.g. affile_sd_init), 
 *   - destroy is called from driver free method (e.g. affile_sd_free, NOT affile_sd_deinit)
 *
 * The reason:
 *   - when initializing the reloaded configuration fails for some reason,
 *     we have to fall back to the old configuration, thus we cannot dump
 *     the information stored in the Options structure.
 *
 * For the reasons above, init and destroy behave the following way:
 *
 *   - init is idempotent, it can be called multiple times without leaking
 *     memory, and without loss of information
 *   - destroy is only called once, when the options are indeed to be destroyed
 *
 * As init allocates memory, it has to take care about freeing memory
 * allocated by the previous init call (or it has to reuse those).
 *   
 */
void
log_reader_options_init(LogReaderOptions *options, GlobalConfig *cfg, const gchar *group_name)
{
  gchar *recv_time_zone;
  TimeZoneInfo *recv_time_zone_info;
  gchar *host_override, *program_override, *text_encoding;
  GArray *tags;

  recv_time_zone = options->parse_options.recv_time_zone;
  options->parse_options.recv_time_zone = NULL;
  recv_time_zone_info = options->parse_options.recv_time_zone_info;
  options->parse_options.recv_time_zone_info = NULL;
  text_encoding = options->text_encoding;
  options->text_encoding = NULL;
  tags = options->tags;
  options->tags = NULL;

  /* NOTE: having to save super's variables is a crude hack, but I know of
   * no other way to do it in the scheme described above. Be sure that you
   * know what you are doing when you modify this code. */
  
  host_override = options->super.host_override;
  options->super.host_override = NULL;
  program_override = options->super.program_override;
  options->super.program_override = NULL;

  log_reader_options_destroy(options);

  options->super.host_override = host_override;
  options->super.program_override = program_override;
  
  options->parse_options.recv_time_zone = recv_time_zone;
  options->parse_options.recv_time_zone_info = recv_time_zone_info;
  options->text_encoding = text_encoding;
  options->tags = tags;

  log_source_options_init(&options->super, cfg, group_name);
  log_parse_syslog_options_init(&options->parse_options, cfg);

  if (options->fetch_limit == -1)
    options->fetch_limit = cfg->log_fetch_limit;
  if (options->msg_size == -1)
    options->msg_size = cfg->log_msg_size;
  if (options->follow_freq == -1)
    options->follow_freq = cfg->follow_freq;
  if (options->check_hostname == -1)
    options->check_hostname = cfg->check_hostname;
  if (options->check_hostname)
    {
      options->parse_options.flags |= LP_CHECK_HOSTNAME;
    }
  if (options->parse_options.default_pri == 0xFFFF)
    {
      if (options->flags & LR_KERNEL)
        options->parse_options.default_pri = LOG_KERN | LOG_NOTICE;
      else
        options->parse_options.default_pri = LOG_USER | LOG_NOTICE;
    }
  if (options->text_encoding)
    options->parse_options.flags |= LP_ASSUME_UTF8;
}

void
log_reader_options_destroy(LogReaderOptions *options)
{
  log_source_options_destroy(&options->super);
  log_parse_syslog_options_destroy(&options->parse_options);
  if (options->text_encoding)
    {
      g_free(options->text_encoding);
      options->text_encoding = NULL;
    }
  if (options->tags)
    {
      g_array_free(options->tags, TRUE);
      options->tags = NULL;
    }
}

CfgFlagHandler log_reader_flag_handlers[] =
{
  /* LogParseOptions */
  { "no-parse",                   CFH_SET, offsetof(LogReaderOptions, parse_options.flags), LP_NOPARSE },
  { "check-hostname",             CFH_SET, offsetof(LogReaderOptions, parse_options.flags), LP_CHECK_HOSTNAME },
  { "syslog-protocol",            CFH_SET, offsetof(LogReaderOptions, parse_options.flags), LP_SYSLOG_PROTOCOL },
  { "assume-utf8",                CFH_SET, offsetof(LogReaderOptions, parse_options.flags), LP_ASSUME_UTF8 },
  { "validate-utf8",              CFH_SET, offsetof(LogReaderOptions, parse_options.flags), LP_VALIDATE_UTF8 },
  { "no-multi-line",              CFH_SET, offsetof(LogReaderOptions, parse_options.flags), LP_NO_MULTI_LINE },
  { "store-legacy-msghdr",        CFH_SET, offsetof(LogReaderOptions, parse_options.flags), LP_STORE_LEGACY_MSGHDR },
  { "dont-store-legacy-msghdr", CFH_CLEAR, offsetof(LogReaderOptions, parse_options.flags), LP_STORE_LEGACY_MSGHDR },
  { "expect-hostname",            CFH_SET, offsetof(LogReaderOptions, parse_options.flags), LP_EXPECT_HOSTNAME },
  { "no-hostname",              CFH_CLEAR, offsetof(LogReaderOptions, parse_options.flags), LP_EXPECT_HOSTNAME },

  /* LogReaderOptions */
  { "kernel",                     CFH_SET, offsetof(LogReaderOptions, flags),               LR_KERNEL },
  { "empty-lines",                CFH_SET, offsetof(LogReaderOptions, flags),               LR_EMPTY_LINES },
};

gboolean
log_reader_options_process_flag(LogReaderOptions *options, gchar *flag)
{
  return cfg_process_flag(log_reader_flag_handlers, options, flag);
}

void
log_reader_options_set_tags(LogReaderOptions *options, GList *tags)
{
  guint id;

  if (!options->tags)
    options->tags = g_array_new(FALSE, FALSE, sizeof(guint));

  while (tags)
    {
      id = log_tags_get_by_name((gchar *) tags->data);
      g_array_append_val(options->tags, id);

      g_free(tags->data);
      tags = g_list_delete_link(tags, tags);
    }
}
