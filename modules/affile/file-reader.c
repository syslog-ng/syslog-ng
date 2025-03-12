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
#include "logproto/logproto-buffered-server.h"
#include "poll-fd-events.h"
#include "poll-file-changes.h"
#include "poll-multiline-file-changes.h"
#include "ack-tracker/ack_tracker_factory.h"
#include "stats/stats-cluster-key-builder.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <glib/gprintf.h>

#include <iv.h>

static inline gint G_GNUC_PRINTF(2, 3)
_format_g_va_string(gchar **strp, const gchar *format, ...)
{
  va_list args;

  va_start(args, format);
  gint result = g_vasprintf(strp, format, args);
  va_end(args);

  return result;
}

static const gchar *
_format_persist_name(const LogPipe *s)
{
  const FileReader *self = (const FileReader *)s;
  guint name_hash = g_str_hash(self->filename->str);
  const gint max_persist_name_len = 1024;
  gchar *persist_name = NULL;

  /* same wildcard file sources even with persist-name defined can be the same, so
     file name hash is added as well to ensure uniquiness always
     also adding now the filename (or at least an end part of it) for debug aid purpose
   */
  gint used_len;
  if (self->owner->super.super.persist_name)
    used_len = _format_g_va_string(&persist_name, "%s.%s.%u.curpos", self->persist_name_prefix,
                                   self->owner->super.super.persist_name, name_hash);
  else
    used_len = _format_g_va_string(&persist_name, "%s.%u.curpos", self->persist_name_prefix, name_hash);
  g_assert(used_len > 0);

  if (used_len < max_persist_name_len - 2 - 1) /* () and the terminating NUL */
    {
      gchar *base_name = persist_name;
      gint filename_len = strlen(self->filename->str);
      gint remaining_len = max_persist_name_len - used_len - 2 - 1;  /* () and the terminating NUL */
      persist_name = NULL; /* glib requires the input ptr to be NULL otherwise returning with -1 from g_vasprintf */
      _format_g_va_string(&persist_name, "%s(%s)", base_name,
                          self->filename->str + MAX(0, filename_len - remaining_len));
      g_free(base_name);
    }

  return persist_name;
}

static inline const gchar *
_generate_persist_name(const LogPipe *s)
{
  FileReader *self = (FileReader *) s;

  if (self->persist_name == NULL)
    {
      if (self->super.generate_persist_name == _generate_persist_name)
        self->persist_name = _format_persist_name(s);
      else
        {
          const gchar *name = log_pipe_get_persist_name(s);
          self->persist_name = name ? g_strdup(name) : _format_persist_name(s);
        }
    }
  return self->persist_name;
}

static inline const gchar *
_format_legacy_persist_name(const LogPipe *s)
{
  const FileReader *self = (const FileReader *)s;
  static gchar persist_name[1024];

  if (self->owner->super.super.persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "affile_sd.%s.curpos", self->owner->super.super.persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "affile_sd_curpos(%s)", self->filename->str);

  return persist_name;
}

static gboolean
_update_legacy_persist_name(FileReader *self)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super);

  if (cfg->state == NULL)
    return TRUE;

  const gchar *current_persist_name = _generate_persist_name(&self->super);
  const gchar *legacy_persist_name = _format_legacy_persist_name(&self->super);

  if (persist_state_entry_exists(cfg->state, current_persist_name))
    return TRUE;

  if (FALSE == persist_state_entry_exists(cfg->state, legacy_persist_name))
    return TRUE;

  return persist_state_copy_entry(cfg->state, legacy_persist_name, current_persist_name);
}

static void
_recover_state(LogPipe *s, GlobalConfig *cfg, LogProtoServer *proto)
{
  FileReader *self = (FileReader *) s;

  if (!self->options->restore_state)
    return;

  if (!log_proto_server_restart_with_state(proto, cfg->state, log_pipe_get_persist_name(s)))
    {
      msg_error("Error converting persistent state from on-disk format, losing file position information",
                evt_tag_str("filename", self->filename->str));
      return;
    }
}

static gboolean
_can_check_eof(FileReader *self, gint fd)
{
  struct stat st;

  if (fstat(fd, &st) == -1 || S_ISFIFO(st.st_mode) || S_ISSOCK(st.st_mode) || S_ISCHR(st.st_mode))
    return FALSE;

  off_t pos = lseek(fd, 0, SEEK_CUR);
  if (pos == -1)
    return FALSE;

  off_t reset = lseek(fd, pos, SEEK_SET);
  if (reset != pos)
    {
      msg_trace("File seek pos is different after testing if seekable",
                evt_tag_str("follow_filename", self->filename->str),
                evt_tag_int("fn", fd));
    }

  return TRUE;
}

static gboolean
_reader_check_eof(FileReader *self, gint fd)
{
  if (fd < 0)
    return FALSE;

  off_t pos = lseek(fd, 0, SEEK_CUR);
  if (pos == (off_t) -1)
    {
      msg_error("Error invoking seek on file",
                evt_tag_str("filename", self->filename->str),
                evt_tag_error("error"));
      return FALSE;
    }

  struct stat st;
  gboolean end_of_file = fstat(fd, &st) == 0 && pos == st.st_size;
  return end_of_file;
}

static inline gboolean
_reader_on_eof(FileReader *self)
{
  gboolean result = TRUE;
  if (log_pipe_notify(&self->super, NC_FILE_EOF, self) & NR_STOP_ON_EOF)
    result = FALSE;
  return result;
}

static inline gboolean
_reader_check_watches(PollEvents *poll_events, gpointer user_data)
{
  FileReader *self = (FileReader *) user_data;
  gboolean check_again = TRUE;
  gint fd = poll_events_get_fd(poll_events);

  if (_reader_check_eof(self, fd))
    {
      msg_trace("End of file, following file",
                evt_tag_str("follow_filename", self->filename->str),
                evt_tag_int("fn", fd));
      check_again = _reader_on_eof(self);
    }
  return check_again;
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
  PollEvents *poll_events = NULL;
  if (self->options->follow_freq > 0)
    {
      LogProtoFileReaderOptions *proto_opts = file_reader_options_get_log_proto_options(self->options);

      if (proto_opts->super.multi_line_options.mode == MLM_NONE)
        poll_events = poll_file_changes_new(fd, self->filename->str, self->options->follow_freq, &self->super);
      else
        poll_events = poll_multiline_file_changes_new(fd, self->filename->str, self->options->follow_freq,
                                                      self->options->multi_line_timeout, self);
      msg_trace("File follow-mode is syslog-ng poll");
    }
  else if (fd >= 0 && _is_fd_pollable(fd))
    {
      poll_events = poll_fd_events_new(fd);
      msg_trace("File follow-mode is ivykis poll");
      msg_trace("Selected ivykis poll method", evt_tag_str("file_poll_method", iv_poll_method_name()));
    }
  else
    {
      msg_error("Unable to determine how to monitor this file, follow_freq() unset and it is not possible to poll it "
                "with the current ivykis polling method. Set follow-freq() for regular files or change "
                "IV_EXCLUDE_POLL_METHOD environment variable to override the automatically selected polling method",
                evt_tag_str("file_poll_method", iv_poll_method_name()),
                evt_tag_str("filename", self->filename->str),
                evt_tag_int("fd", fd));
      return NULL;
    }

  if (_can_check_eof(self, fd))
    poll_events_set_checker(poll_events, _reader_check_watches, self);

  return poll_events;
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
  LogTransport *transport = _construct_transport(self, fd);
  MsgFormatHandler *format_handler = reader_options->parse_options.format_handler;

  if ((format_handler && format_handler->construct_proto))
    {
      log_proto_server_options_set_ack_tracker_factory(&proto_options->storage,
                                                       consecutive_ack_tracker_factory_new());
      return format_handler->construct_proto(&reader_options->parse_options, transport,
                                             &proto_options->storage);
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
  log_pipe_set_options(&self->reader->super.super, &self->super.options);
  log_reader_open(self->reader, proto, poll_events);

  StatsClusterKeyBuilder *kb = stats_cluster_key_builder_new();
  stats_cluster_key_builder_add_label(kb, stats_cluster_label("driver", "file"));
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("filename", self->filename->str));
  log_reader_set_options(self->reader,
                         s,
                         &self->options->reader_options,
                         self->owner->super.id,
                         kb);

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
  gboolean open_deferred = FALSE;

  FileOpenerResult res = file_opener_open_fd(self->opener, self->filename->str, AFFILE_DIR_READ, &fd);
  gboolean file_opened =  res == FILE_OPENER_RESULT_SUCCESS;

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

static void
_on_file_close(FileReader *self)
{
  if (self->options->exit_on_eof)
    cfg_shutdown(log_pipe_get_config(&self->super));
}

static void
_on_file_moved(FileReader *self)
{
  msg_verbose("Follow-mode file source moved, tracking of the new file is started",
              evt_tag_str("filename", self->filename->str));
  if (self->on_file_moved)
    self->on_file_moved(self);
  else
    _reopen_on_notify(&self->super, TRUE);
}

static void
_on_file_deleted(FileReader *self)
{
  /* We want to handle the events like file deleted in the same workflow.
   * Manually polled file readers like poll-file-changes can handle this case,
   * read the remaining file content to the end, and signal the EOF, this is
   * happening from poll_events_update_watches, but that one is not always triggered
   * automatically for system file event notifications (like poll-fd-events), as there
   * might be no more file events after the file is fully read.
   * So, we have to trigger one more read.
   * NOTE: Do not try to close the reader directly from here, as there might already be
   *       an io-operation in progress!
   */
  if (poll_events_system_polled(self->reader->poll_events))
    log_reader_trigger_one_check(self->reader);
}

static void
_on_read_error(FileReader *self)
{
  msg_verbose("Error while following source file, reopening in the hope it would work",
              evt_tag_str("filename", self->filename->str));
  _reopen_on_notify(&self->super, FALSE);
}

/* NOTE: runs in the main thread */
gint
file_reader_notify_method(LogPipe *s, gint notify_code, gpointer user_data)
{
  FileReader *self = (FileReader *) s;

  switch (notify_code)
    {
    case NC_CLOSE:
      _on_file_close(self);
      break;

    case NC_FILE_MOVED:
      _on_file_moved(self);
      break;

    case NC_READ_ERROR:
      _on_read_error(self);
      break;

    case NC_FILE_DELETED:
      _on_file_deleted(self);
      break;

    default:
      break;
    }
  return NR_OK;
}

void
file_reader_queue_method(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  FileReader *self = (FileReader *)s;

  log_msg_set_value(msg, LM_V_FILE_NAME, self->filename->str, self->filename->len);
  log_pipe_forward_msg(s, msg, path_options);
}

gboolean
file_reader_init_method(LogPipe *s)
{
  _update_legacy_persist_name((FileReader *)s);

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
  g_free((gpointer)self->persist_name);
}

void
file_reader_remove_persist_state(FileReader *self)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super);
  const gchar *old_persist_name = log_pipe_get_persist_name(&self->super);
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
file_reader_cue_buffer_flush(FileReader *self)
{
  LogProtoBufferedServer *proto = (LogProtoBufferedServer *) self->reader->proto;
  log_proto_buffered_server_cue_flush(proto);
}

void
file_reader_init_instance (FileReader *self, const gchar *filename,
                           FileReaderOptions *options, FileOpener *opener,
                           LogSrcDriver *owner, GlobalConfig *cfg,
                           const gchar *persist_name_prefix)
{
  log_pipe_init_instance (&self->super, cfg);
  self->super.init = file_reader_init_method;
  self->super.queue = file_reader_queue_method;
  self->super.deinit = file_reader_deinit_method;
  self->super.notify = file_reader_notify_method;
  self->super.free_fn = file_reader_free_method;
  self->super.generate_persist_name = _generate_persist_name;
  self->persist_name_prefix = persist_name_prefix;
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

  file_reader_init_instance (self, filename, options, opener, owner, cfg, "affile_sd");
  return self;
}

void
file_reader_options_set_follow_freq(FileReaderOptions *options, gint follow_freq)
{
  options->follow_freq = follow_freq;
}

void
file_reader_options_set_multi_line_timeout(FileReaderOptions *options, gint multi_line_timeout)
{
  options->multi_line_timeout = multi_line_timeout;
}

void
file_reader_options_defaults(FileReaderOptions *options)
{
  log_reader_options_defaults(&options->reader_options);
  log_proto_file_reader_options_defaults(file_reader_options_get_log_proto_options(options));
  options->reader_options.parse_options.flags |= LP_LOCAL;
  options->restore_state = FALSE;
}

static gboolean
file_reader_options_validate(FileReaderOptions *options)
{
  if (options->multi_line_timeout && options->follow_freq >= options->multi_line_timeout)
    {
      msg_error("multi-line-timeout() should be set to a higher value than follow-freq(), "
                "it is recommended to set multi-line-timeout() to a multiple of follow-freq()",
                evt_tag_int("multi_line_timeout", options->multi_line_timeout),
                evt_tag_int("follow_freq", options->follow_freq));
      return FALSE;
    }

  return TRUE;
}

gboolean
file_reader_options_init(FileReaderOptions *options, GlobalConfig *cfg, const gchar *group)
{
  log_reader_options_init(&options->reader_options, cfg, group);

  if (!file_reader_options_validate(options))
    return FALSE;

  return log_proto_file_reader_options_init(file_reader_options_get_log_proto_options(options), cfg);
}

void
file_reader_options_deinit(FileReaderOptions *options)
{
  log_reader_options_destroy(&options->reader_options);
}
