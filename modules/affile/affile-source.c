/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
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
#include "affile-common.h"
#include "affile-source.h"
#include "driver.h"
#include "messages.h"
#include "misc.h"
#include "serialize.h"
#include "gprocess.h"
#include "stats.h"
#include "mainloop.h"
#include "logproto-record-server.h"
#include "logproto-text-server.h"
#include "logproto-linux-proc-kmsg-reader.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>

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
affile_is_device_node(const gchar *filename)
{
  struct stat st;

  if (stat(filename, &st) < 0)
    return FALSE;
  return !S_ISREG(st.st_mode);
}

static gboolean
affile_sd_open_file(AFFileSourceDriver *self, gchar *name, gint *fd)
{
  gint flags;
  
  if (self->flags & AFFILE_PIPE)
    flags = O_RDWR | O_NOCTTY | O_NONBLOCK | O_LARGEFILE;
  else
    flags = O_RDONLY | O_NOCTTY | O_NONBLOCK | O_LARGEFILE;

  if (affile_open_file(name, flags,
                       &self->file_perm_options,
                       0, !!(self->flags & AFFILE_PRIVILEGED), !!(self->flags & AFFILE_PIPE), fd))
    return TRUE;
  return FALSE;
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

  if ((self->flags & AFFILE_PIPE) || self->reader_options.follow_freq <= 0)
    return;

  if (!log_proto_server_restart_with_state(proto, cfg->state, affile_sd_format_persist_name(self)))
    {
      msg_error("Error converting persistent state from on-disk format, losing file position information",
                evt_tag_str("filename", self->filename->str),
                NULL);
      return;
    }
}

static LogTransport *
affile_sd_construct_transport(AFFileSourceDriver *self, gint fd)
{
  if (self->flags & AFFILE_PIPE)
    return log_transport_pipe_new(fd);
  else if (self->reader_options.follow_freq > 0)
    return log_transport_file_new(fd);
  else
    return log_transport_device_new(fd, 10);
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
  else
    return log_proto_text_server_new(transport, proto_options);
}

/* NOTE: runs in the main thread */
static void
affile_sd_notify(LogPipe *s, LogPipe *sender, gint notify_code, gpointer user_data)
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
        
        log_pipe_deinit(self->reader);
        log_pipe_unref(self->reader);
        
        if (affile_sd_open_file(self, self->filename->str, &fd))
          {
            LogProtoServer *proto;
            
            proto = affile_sd_construct_proto(self, fd);

            self->reader = log_reader_new(proto);

            log_reader_set_options(self->reader, s, &self->reader_options, 1, SCS_FILE, self->super.super.id, self->filename->str);
            log_reader_set_follow_filename(self->reader, self->filename->str);
            log_reader_set_immediate_check(self->reader);

            log_pipe_append(self->reader, s);
            if (!log_pipe_init(self->reader, cfg))
              {
                msg_error("Error initializing log_reader, closing fd",
                          evt_tag_int("fd", fd),
                          NULL);
                log_pipe_unref(self->reader);
                self->reader = NULL;
                close(fd);
              }
            affile_sd_recover_state(s, cfg, proto);
          }
        else
          {
            self->reader = NULL;
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

  log_pipe_forward_msg(s, msg, path_options);
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

  file_opened = affile_sd_open_file(self, self->filename->str, &fd);
  if (!file_opened && self->reader_options.follow_freq > 0)
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

      proto = affile_sd_construct_proto(self, fd);
      /* FIXME: we shouldn't use reader_options to store log protocol parameters */
      self->reader = log_reader_new(proto);

      log_reader_set_options(self->reader, s, &self->reader_options, 1, SCS_FILE, self->super.super.id, self->filename->str);
      log_reader_set_follow_filename(self->reader, self->filename->str);

      /* NOTE: if the file could not be opened, we ignore the last
       * remembered file position, if the file is created in the future
       * we're going to read from the start. */
      
      log_pipe_append(self->reader, s);

      if (!log_pipe_init(self->reader, NULL))
        {
          msg_error("Error initializing log_reader, closing fd",
                    evt_tag_int("fd", fd),
                    NULL);
          log_pipe_unref(self->reader);
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
      log_pipe_deinit(self->reader);
      log_pipe_unref(self->reader);
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

  log_src_driver_free(s);
}

LogDriver *
affile_sd_new(gchar *filename, guint32 flags)
{
  AFFileSourceDriver *self = g_new0(AFFileSourceDriver, 1);
  
  log_src_driver_init_instance(&self->super);
  self->filename = g_string_new(filename);
  self->flags = flags;
  self->super.super.super.init = affile_sd_init;
  self->super.super.super.queue = affile_sd_queue;
  self->super.super.super.deinit = affile_sd_deinit;
  self->super.super.super.notify = affile_sd_notify;
  self->super.super.super.free_fn = affile_sd_free;
  log_reader_options_defaults(&self->reader_options);
  file_perm_options_defaults(&self->file_perm_options);
  self->reader_options.parse_options.flags |= LP_LOCAL;

  if ((self->flags & AFFILE_PIPE))
    {
      static gboolean warned = FALSE;

      if (cfg_is_config_version_older(configuration, 0x0302))
        {
          if (!warned)
            {
              msg_warning("WARNING: the expected message format is being changed for pipe() to improve "
                          "syslogd compatibity with " VERSION_3_2 ". If you are using custom "
                          "applications which bypass the syslog() API, you might "
                          "need the 'expect-hostname' flag to get the old behaviour back", NULL);
              warned = TRUE;
            }
        }
      else
        {
          self->reader_options.parse_options.flags &= ~LP_EXPECT_HOSTNAME;
        }
    }
  
  if (cfg_is_config_version_older(configuration, 0x0300))
    {
      static gboolean warned = FALSE;
      
      if (!warned)
        {
          msg_warning("WARNING: file source: default value of follow_freq in file sources has changed in " VERSION_3_0 " to '1' for all files except /proc/kmsg",
                      NULL);
          warned = TRUE;
        }
    }
  else
    {
      if ((self->flags & AFFILE_PIPE) == 0)
        {
          if (affile_is_device_node(filename) ||
              affile_is_linux_proc_kmsg(filename))
            self->reader_options.follow_freq = 0;
          else
            self->reader_options.follow_freq = 1000;
        }
    }
  if (affile_is_linux_proc_kmsg(filename))
    self->flags |= AFFILE_PRIVILEGED;
  return &self->super.super;
}
