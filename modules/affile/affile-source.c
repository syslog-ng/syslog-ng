/*
 * Copyright (c) 2002-2014 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#include "affile-source.h"
#include "driver.h"
#include "messages.h"
#include "serialize.h"
#include "gprocess.h"
#include "stats/stats-registry.h"
#include "mainloop.h"
#include "transport/transport-file.h"
#include "transport/transport-pipe.h"
#include "transport/transport-device.h"
#include "logproto/logproto-record-server.h"
#include "logproto/logproto-text-server.h"
#include "logproto/logproto-dgram-server.h"
#include "logproto/logproto-indented-multiline-server.h"
#include "logproto-linux-proc-kmsg-reader.h"
#include "poll-fd-events.h"
#include "poll-file-changes.h"
#include "compat/lfs.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>

#include <iv.h>

#define DEFAULT_SD_OPEN_FLAGS (O_RDONLY | O_NOCTTY | O_NONBLOCK | O_LARGEFILE)
#define DEFAULT_SD_OPEN_FLAGS_PIPE (O_RDWR | O_NOCTTY | O_NONBLOCK | O_LARGEFILE)

gboolean
affile_sd_set_multi_line_mode(LogDriver *s, const gchar *mode)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;

  if (strcasecmp(mode, "indented") == 0)
    self->multi_line_mode = MLM_INDENTED;
  else if (strcasecmp(mode, "regexp") == 0)
    self->multi_line_mode = MLM_PREFIX_GARBAGE;
  else if (strcasecmp(mode, "prefix-garbage") == 0)
    self->multi_line_mode = MLM_PREFIX_GARBAGE;
  else if (strcasecmp(mode, "prefix-suffix") == 0)
    self->multi_line_mode = MLM_PREFIX_SUFFIX;
  else if (strcasecmp(mode, "none") == 0)
    self->multi_line_mode = MLM_NONE;
  else
    return FALSE;
  return TRUE;
}

gboolean
affile_sd_set_multi_line_prefix(LogDriver *s, const gchar *prefix_regexp, GError **error)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;

  self->multi_line_prefix = multi_line_regexp_compile(prefix_regexp, error);
  return self->multi_line_prefix != NULL;
}

gboolean
affile_sd_set_multi_line_garbage(LogDriver *s, const gchar *garbage_regexp, GError **error)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;

  self->multi_line_garbage = multi_line_regexp_compile(garbage_regexp, error);
  return self->multi_line_garbage != NULL;
}

void
affile_sd_set_follow_freq(LogDriver *s, gint follow_freq)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;

  self->follow_freq = follow_freq;
}

static inline gboolean
affile_is_linux_proc_kmsg(const gchar *filename)
{
#ifdef __linux__
  if (strcmp(filename, "/proc/kmsg") == 0)
    return TRUE;
#endif
  return FALSE;
}

static inline gboolean
affile_is_linux_dev_kmsg(const gchar *filename)
{
#ifdef __linux__
  if (strcmp(filename, "/dev/kmsg") == 0)
    return TRUE;
#endif
  return FALSE;
}

static inline gboolean
affile_is_device_node(const gchar *filename)
{
  struct stat st;

  if (stat(filename, &st) < 0)
    return FALSE;
  return !S_ISREG(st.st_mode);
}

gboolean
affile_sd_open_file(AFFileSourceDriver *self, gchar *name, gint *fd)
{
  return affile_open_file(name, &self->file_open_options, &self->file_perm_options, fd);
}

static inline gchar *
affile_sd_format_persist_name(AFFileSourceDriver *self)
{
  static gchar persist_name[1024];
  
  g_snprintf(persist_name, sizeof(persist_name), "affile_sd_curpos(%s)", self->filename->str);
  return persist_name;
}
 
static void
affile_sd_recover_state(LogPipe *s, GlobalConfig *cfg, LogProtoServer *proto)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;

  if (self->file_open_options.is_pipe || self->follow_freq <= 0)
    return;

  if (!log_proto_server_restart_with_state(proto, cfg->state, affile_sd_format_persist_name(self)))
    {
      msg_error("Error converting persistent state from on-disk format, losing file position information",
                evt_tag_str("filename", self->filename->str),
                NULL);
      return;
    }
}

static gboolean
_is_fd_pollable(gint fd)
{
  struct iv_fd check_fd;
  gboolean pollable;

  IV_FD_INIT(&check_fd);
  check_fd.fd = fd;
  check_fd.cookie = NULL;

  pollable = (iv_fd_register_try(&check_fd) == 0);
  if (pollable)
    iv_fd_unregister(&check_fd);
  return pollable;
}

static PollEvents *
affile_sd_construct_poll_events(AFFileSourceDriver *self, gint fd)
{
  if (self->follow_freq > 0)
    return poll_file_changes_new(fd, self->filename->str, self->follow_freq, &self->super.super.super);
  else if (fd >= 0 && _is_fd_pollable(fd))
    return poll_fd_events_new(fd);
  else
    {
      msg_error("Unable to determine how to monitor this file, follow_freq() unset and it is not possible to poll it with the current ivykis polling method. Set follow-freq() for regular files or change IV_EXCLUDE_POLL_METHOD environment variable to override the automatically selected polling method",
                evt_tag_str("filename", self->filename->str),
                evt_tag_int("fd", fd),
                NULL);
      return NULL;
    }
}

static LogTransport *
affile_sd_construct_transport(AFFileSourceDriver *self, gint fd)
{
  if (self->file_open_options.is_pipe)
    return log_transport_pipe_new(fd);
  else if (self->follow_freq > 0)
    return log_transport_file_new(fd);
  else if (affile_is_linux_proc_kmsg(self->filename->str))
    return log_transport_device_new(fd, 10);
  else if (affile_is_linux_dev_kmsg(self->filename->str))
    {
      if (lseek(fd, 0, SEEK_END) < 0)
        {
          msg_error("Error seeking /dev/kmsg to the end",
                    evt_tag_str("error", g_strerror(errno)),
                    NULL);
        }
      return log_transport_device_new(fd, 0);
    }
  else
    return log_transport_pipe_new(fd);
}

static LogProtoServer *
affile_sd_construct_proto(AFFileSourceDriver *self, gint fd)
{
  LogProtoServerOptions *proto_options = &self->reader_options.proto_options.super;
  LogTransport *transport;
  MsgFormatHandler *format_handler;

  transport = affile_sd_construct_transport(self, fd);

  format_handler = self->reader_options.parse_options.format_handler;
  if ((format_handler && format_handler->construct_proto))
    {
      return format_handler->construct_proto(&self->reader_options.parse_options, transport, proto_options);
    }

  if (self->pad_size)
    return log_proto_padded_record_server_new(transport, proto_options, self->pad_size);
  else if (affile_is_linux_proc_kmsg(self->filename->str))
    return log_proto_linux_proc_kmsg_reader_new(transport, proto_options);
  else if (affile_is_linux_dev_kmsg(self->filename->str))
    return log_proto_dgram_server_new(transport, proto_options);
  else
    {
      switch (self->multi_line_mode)
        {
        case MLM_INDENTED:
          return log_proto_indented_multiline_server_new(transport, proto_options);
        case MLM_PREFIX_GARBAGE:
          return log_proto_prefix_garbage_multiline_server_new(transport, proto_options, self->multi_line_prefix, self->multi_line_garbage);
        case MLM_PREFIX_SUFFIX:
          return log_proto_prefix_suffix_multiline_server_new(transport, proto_options, self->multi_line_prefix, self->multi_line_garbage);
        default:
          return log_proto_text_server_new(transport, proto_options);
        }
    }
}

/* NOTE: runs in the main thread */
static void
affile_sd_notify(LogPipe *s, gint notify_code, gpointer user_data)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);
  gint fd;
  
  switch (notify_code)
    {
    case NC_FILE_MOVED:
      { 
        msg_verbose("Follow-mode file source moved, tracking of the new file is started",
                    evt_tag_str("filename", self->filename->str),
                    NULL);
        
        log_pipe_deinit((LogPipe *) self->reader);
        log_pipe_unref((LogPipe *) self->reader);
        self->reader = NULL;
        
        if (affile_sd_open_file(self, self->filename->str, &fd))
          {
            LogProtoServer *proto;
            PollEvents *poll_events;
            
            poll_events = affile_sd_construct_poll_events(self, fd);
            if (!poll_events)
              break;

            proto = affile_sd_construct_proto(self, fd);

            self->reader = log_reader_new(self->super.super.super.cfg);
            log_reader_reopen(self->reader, proto, poll_events);

            log_reader_set_options(self->reader,
                                   s,
                                   &self->reader_options,
                                   STATS_LEVEL1,
                                   SCS_FILE,
                                   self->super.super.id,
                                   self->filename->str);
            log_reader_set_immediate_check(self->reader);

            log_pipe_append((LogPipe *) self->reader, s);
            if (!log_pipe_init((LogPipe *) self->reader))
              {
                msg_error("Error initializing log_reader, closing fd",
                          evt_tag_int("fd", fd),
                          NULL);
                log_pipe_unref((LogPipe *) self->reader);
                self->reader = NULL;
                close(fd);
              }
            affile_sd_recover_state(s, cfg, proto);
          }
        break;
      }
    default:
      break;
    }
}

static void
affile_sd_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options, gpointer user_data)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;
  static NVHandle filename_handle = 0;

  if (!filename_handle)
    filename_handle = log_msg_get_value_handle("FILE_NAME");
  
  log_msg_set_value(msg, filename_handle, self->filename->str, self->filename->len);

  log_src_driver_queue_method(s, msg, path_options, user_data);
}

static gboolean
affile_sd_init(LogPipe *s)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);
  gint fd;
  gboolean file_opened, open_deferred = FALSE;

  if (!log_src_driver_init_method(s))
    return FALSE;

  log_reader_options_init(&self->reader_options, cfg, self->super.super.group);

  if ((self->multi_line_mode != MLM_PREFIX_GARBAGE && self->multi_line_mode != MLM_PREFIX_SUFFIX ) && (self->multi_line_prefix || self->multi_line_garbage))
    {
      msg_error("multi-line-prefix() and/or multi-line-garbage() specified but multi-line-mode() is not regexp based (prefix-garbage or prefix-suffix), please set multi-line-mode() properly", NULL);
      return FALSE;
    }

  file_opened = affile_sd_open_file(self, self->filename->str, &fd);
  if (!file_opened && self->follow_freq > 0)
    {
      msg_info("Follow-mode file source not found, deferring open",
               evt_tag_str("filename", self->filename->str),
               NULL);
      open_deferred = TRUE;
      fd = -1;
    }

  if (file_opened || open_deferred)
    {
      LogProtoServer *proto;
      PollEvents *poll_events;

      poll_events = affile_sd_construct_poll_events(self, fd);
      if (!poll_events)
        {
          close(fd);
          return FALSE;
        }

      proto = affile_sd_construct_proto(self, fd);
      self->reader = log_reader_new(self->super.super.super.cfg);
      log_reader_reopen(self->reader, proto, poll_events);

      log_reader_set_options(self->reader,
                             s,
                             &self->reader_options,
                             STATS_LEVEL1,
                             SCS_FILE,
                             self->super.super.id,
                             self->filename->str);
      /* NOTE: if the file could not be opened, we ignore the last
       * remembered file position, if the file is created in the future
       * we're going to read from the start. */
      
      log_pipe_append((LogPipe *) self->reader, s);
      if (!log_pipe_init((LogPipe *) self->reader))
        {
          msg_error("Error initializing log_reader, closing fd",
                    evt_tag_int("fd", fd),
                    NULL);
          log_pipe_unref((LogPipe *) self->reader);
          self->reader = NULL;
          close(fd);
          return FALSE;
        }
      affile_sd_recover_state(s, cfg, proto);
    }
  else
    {
      msg_error("Error opening file for reading",
                evt_tag_str("filename", self->filename->str),
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                NULL);
      return self->super.super.optional;
    }
  return TRUE;

}

static gboolean
affile_sd_deinit(LogPipe *s)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;

  if (self->reader)
    {
      log_pipe_deinit((LogPipe *) self->reader);
      log_pipe_unref((LogPipe *) self->reader);
      self->reader = NULL;
    }

  if (!log_src_driver_deinit_method(s))
    return FALSE;

  return TRUE;
}

static void
affile_sd_free(LogPipe *s)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;

  g_string_free(self->filename, TRUE);
  g_assert(!self->reader);

  log_reader_options_destroy(&self->reader_options);

  multi_line_regexp_free(self->multi_line_prefix);
  multi_line_regexp_free(self->multi_line_garbage);

  log_src_driver_free(s);
}

static AFFileSourceDriver *
affile_sd_new_instance(gchar *filename, GlobalConfig *cfg)
{
  AFFileSourceDriver *self = g_new0(AFFileSourceDriver, 1);
  
  log_src_driver_init_instance(&self->super, cfg);
  self->filename = g_string_new(filename);
  self->super.super.super.init = affile_sd_init;
  self->super.super.super.queue = affile_sd_queue;
  self->super.super.super.deinit = affile_sd_deinit;
  self->super.super.super.notify = affile_sd_notify;
  self->super.super.super.free_fn = affile_sd_free;
  log_reader_options_defaults(&self->reader_options);
  file_perm_options_defaults(&self->file_perm_options);
  self->reader_options.parse_options.flags |= LP_LOCAL;

  if (affile_is_linux_proc_kmsg(filename))
    self->file_open_options.needs_privileges = TRUE;
  return self;
}

LogDriver *
affile_sd_new(gchar *filename, GlobalConfig *cfg)
{
  AFFileSourceDriver *self = affile_sd_new_instance(filename, cfg);

  self->file_open_options.is_pipe = FALSE;
  self->file_open_options.open_flags = DEFAULT_SD_OPEN_FLAGS;

  if (cfg_is_config_version_older(cfg, 0x0300))
    {
      msg_warning_once("WARNING: file source: default value of follow_freq in file sources has changed in " VERSION_3_0 " to '1' for all files except /proc/kmsg",
                       NULL);
      self->follow_freq = -1;
    }
  else
    {
      if (affile_is_device_node(filename) ||
          affile_is_linux_proc_kmsg(filename))
        self->follow_freq = 0;
      else
        self->follow_freq = 1000;
    }

  return &self->super.super;
}

LogDriver *
afpipe_sd_new(gchar *filename, GlobalConfig *cfg)
{
  AFFileSourceDriver *self = affile_sd_new_instance(filename, cfg);

  self->file_open_options.is_pipe = TRUE;
  self->file_open_options.open_flags = DEFAULT_SD_OPEN_FLAGS_PIPE;

  if (cfg_is_config_version_older(cfg, 0x0302))
    {
      msg_warning_once("WARNING: the expected message format is being changed for pipe() to improve "
                       "syslogd compatibity with " VERSION_3_2 ". If you are using custom "
                       "applications which bypass the syslog() API, you might "
                       "need the 'expect-hostname' flag to get the old behaviour back", NULL);
    }
  else
    {
      self->reader_options.parse_options.flags &= ~LP_EXPECT_HOSTNAME;
    }

  return &self->super.super;
}
