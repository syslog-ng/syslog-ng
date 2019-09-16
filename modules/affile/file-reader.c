/*
 * Copyright (c) 2017 Balabit
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

#include "file-reader.h"
#include "driver.h"
#include "messages.h"
#include "serialize.h"
#include "gprocess.h"
#include "stats/stats-registry.h"
#include "transport/transport-file.h"
#include "transport/transport-pipe.h"
#include "transport-prockmsg.h"
#include "poll-fd-events.h"
#include "poll-file-changes.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>

#include <iv.h>

static inline const gchar *
_format_persist_name(const LogPipe *s)
{
  const FileReader *self = (const FileReader *)s;
  static gchar persist_name[1024];

  if (self->owner->super.super.persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "affile_sd.%s.curpos", self->owner->super.super.persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "affile_sd_curpos(%s)", self->filename->str);

  return persist_name;
}

static void
_recover_state(LogPipe *s, GlobalConfig *cfg, LogProtoServer *proto)
{
  FileReader *self = (FileReader *) s;

  if (!self->options->restore_state)
    return;

  if (!log_proto_server_restart_with_state(proto, cfg->state, _format_persist_name(s)))
    {
      msg_error("Error converting persistent state from on-disk format, losing file position information",
                evt_tag_str("filename", self->filename->str));
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
_construct_poll_events(FileReader *self, gint fd)
{
  if (self->options->follow_freq > 0)
    return poll_file_changes_new(fd, self->filename->str, self->options->follow_freq, &self->super);
  else if (fd >= 0 && _is_fd_pollable(fd))
    return poll_fd_events_new(fd);
  else
    {
      msg_error("Unable to determine how to monitor this file, follow_freq() unset and it is not possible to poll it "
                "with the current ivykis polling method. Set follow-freq() for regular files or change "
                "IV_EXCLUDE_POLL_METHOD environment variable to override the automatically selected polling method",
                evt_tag_str("filename", self->filename->str),
                evt_tag_int("fd", fd));
      return NULL;
    }
}

static LogTransport *
_construct_transport(FileReader *self, gint fd)
{
  return file_opener_construct_transport(self->opener, fd);
}

static LogProtoServer *
_construct_proto(FileReader *self, gint fd)
{
  LogReaderOptions *reader_options = &self->options->reader_options;
  LogProtoFileReaderOptions *proto_options = file_reader_options_get_log_proto_options(self->options);
  LogTransport *transport;
  MsgFormatHandler *format_handler;

  transport = _construct_transport(self, fd);

  format_handler = reader_options->parse_options.format_handler;
  if ((format_handler && format_handler->construct_proto))
    {
      proto_options->super.super.position_tracking_enabled = TRUE;
      return format_handler->construct_proto(&reader_options->parse_options, transport, &proto_options->super.super);
    }

  return file_opener_construct_src_proto(self->opener, transport, proto_options);
}

static void
_deinit_sd_logreader(FileReader *self)
{
  log_pipe_deinit((LogPipe *) self->reader);
  log_pipe_unref((LogPipe *) self->reader);
  self->reader = NULL;
}

static void
_setup_logreader(LogPipe *s, PollEvents *poll_events, LogProtoServer *proto, gboolean check_immediately)
{
  FileReader *self = (FileReader *) s;

  self->reader = log_reader_new(log_pipe_get_config(s));
  log_reader_open(self->reader, proto, poll_events);
  log_reader_set_options(self->reader,
                         s,
                         &self->options->reader_options,
                         self->owner->super.id,
                         self->filename->str);

  if (check_immediately)
    log_reader_set_immediate_check(self->reader);

  /* NOTE: if the file could not be opened, we ignore the last
   * remembered file position, if the file is created in the future
   * we're going to read from the start. */
  log_pipe_append((LogPipe *) self->reader, s);
}

static gboolean
_is_immediate_check_needed(gboolean file_opened, gboolean open_deferred)
{
  if (file_opened)
    return TRUE;
  else if (open_deferred)
    return FALSE;
  return FALSE;
}

static gboolean
_reader_open_file(LogPipe *s, gboolean recover_state)
{
  FileReader *self = (FileReader *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);
  gint fd;
  gboolean file_opened, open_deferred = FALSE;

  file_opened = file_opener_open_fd(self->opener, self->filename->str, AFFILE_DIR_READ, &fd);
  if (!file_opened && self->options->follow_freq > 0)
    {
      msg_info("Follow-mode file source not found, deferring open",
               evt_tag_str("filename", self->filename->str));
      open_deferred = TRUE;
      fd = -1;
    }

  if (file_opened || open_deferred)
    {
      LogProtoServer *proto;
      PollEvents *poll_events;
      gboolean check_immediately;

      poll_events = _construct_poll_events(self, fd);
      if (!poll_events)
        {
          close(fd);
          return FALSE;
        }
      proto = _construct_proto(self, fd);

      check_immediately = _is_immediate_check_needed(file_opened, open_deferred);
      _setup_logreader(s, poll_events, proto, check_immediately);
      if (!log_pipe_init((LogPipe *) self->reader))
        {
          msg_error("Error initializing log_reader, closing fd",
                    evt_tag_int("fd", fd));
          log_pipe_unref((LogPipe *) self->reader);
          self->reader = NULL;
          close(fd);
          return FALSE;
        }
      if (recover_state)
        _recover_state(s, cfg, proto);
    }
  else
    {
      msg_error("Error opening file for reading",
                evt_tag_str("filename", self->filename->str),
                evt_tag_error(EVT_TAG_OSERROR));
      return self->owner->super.optional;
    }
  return TRUE;

}

static void
_reopen_on_notify(LogPipe *s, gboolean recover_state)
{
  FileReader *self = (FileReader *) s;

  _deinit_sd_logreader(self);
  _reader_open_file(s, recover_state);
}

/* NOTE: runs in the main thread */
void
file_reader_notify_method(LogPipe *s, gint notify_code, gpointer user_data)
{
  FileReader *self = (FileReader *) s;

  switch (notify_code)
    {
    case NC_CLOSE:
      if (self->options->exit_on_eof)
        cfg_shutdown(log_pipe_get_config(s));
      break;

    case NC_FILE_MOVED:
      msg_verbose("Follow-mode file source moved, tracking of the new file is started",
                  evt_tag_str("filename", self->filename->str));
      _reopen_on_notify(s, TRUE);
      break;

    case NC_READ_ERROR:
      msg_verbose("Error while following source file, reopening in the hope it would work",
                  evt_tag_str("filename", self->filename->str));
      _reopen_on_notify(s, FALSE);
      break;

    default:
      break;
    }
}

void
file_reader_queue_method(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  FileReader *self = (FileReader *)s;
  static NVHandle filename_handle = 0;

  if (!filename_handle)
    filename_handle = log_msg_get_value_handle("FILE_NAME");

  log_msg_set_value(msg, filename_handle, self->filename->str, self->filename->len);
  log_pipe_forward_msg(s, msg, path_options);
}

gboolean
file_reader_init_method(LogPipe *s)
{
  return _reader_open_file(s, TRUE);
}

gboolean
file_reader_deinit_method(LogPipe *s)
{
  FileReader *self = (FileReader *)s;
  if (self->reader)
    _deinit_sd_logreader(self);
  return TRUE;
}

void
file_reader_free_method(LogPipe *s)
{
  FileReader *self = (FileReader *) s;

  g_assert(!self->reader);
  g_string_free(self->filename, TRUE);
}

void
file_reader_remove_persist_state(FileReader *self)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super);
  const gchar *old_persist_name = _format_persist_name(&self->super);
  gchar *new_persist_name = g_strdup_printf("%s_REMOVED", old_persist_name);
  /* This is required to clean the persist entry from file during restart */
  persist_state_remove_entry(cfg->state, old_persist_name);
  /* This is required to clean the runtime persist state */
  persist_state_rename_entry(cfg->state, old_persist_name, new_persist_name);
  g_free(new_persist_name);
}

void
file_reader_stop_follow_file(FileReader *self)
{
  log_reader_disable_bookmark_saving(self->reader);
  log_reader_close_proto(self->reader);
}

void
file_reader_init_instance (FileReader *self, const gchar *filename,
                           FileReaderOptions *options, FileOpener *opener,
                           LogSrcDriver *owner, GlobalConfig *cfg)
{
  log_pipe_init_instance (&self->super, cfg);
  self->super.init = file_reader_init_method;
  self->super.queue = file_reader_queue_method;
  self->super.deinit = file_reader_deinit_method;
  self->super.notify = file_reader_notify_method;
  self->super.free_fn = file_reader_free_method;
  self->super.generate_persist_name = _format_persist_name;
  self->filename = g_string_new (filename);
  self->options = options;
  self->opener = opener;
  self->owner = owner;
  self->super.expr_node = owner->super.super.expr_node;
}

FileReader *
file_reader_new(const gchar *filename, FileReaderOptions *options, FileOpener *opener, LogSrcDriver *owner,
                GlobalConfig *cfg)
{
  FileReader *self = g_new0(FileReader, 1);

  file_reader_init_instance (self, filename, options, opener, owner, cfg);
  return self;
}

void
file_reader_options_set_follow_freq(FileReaderOptions *options, gint follow_freq)
{
  options->follow_freq = follow_freq;
}

void
file_reader_options_defaults(FileReaderOptions *options)
{
  log_reader_options_defaults(&options->reader_options);
  log_proto_file_reader_options_defaults(file_reader_options_get_log_proto_options(options));
  options->reader_options.parse_options.flags |= LP_LOCAL;
  options->restore_state = FALSE;
}

gboolean
file_reader_options_init(FileReaderOptions *options, GlobalConfig *cfg, const gchar *group)
{
  log_reader_options_init(&options->reader_options, cfg, group);
  return log_proto_file_reader_options_init(file_reader_options_get_log_proto_options(options));
}

void
file_reader_options_deinit(FileReaderOptions *options)
{
  log_reader_options_destroy(&options->reader_options);
  log_proto_file_reader_options_destroy(file_reader_options_get_log_proto_options(options));
}
