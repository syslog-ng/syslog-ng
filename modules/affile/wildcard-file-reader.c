/*
 * Copyright (c) 2018 Balabit
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

#include "wildcard-file-reader.h"
#include "mainloop.h"

static gboolean
_init(LogPipe *s)
{
  WildcardFileReader *self = (WildcardFileReader *)s;
  self->file_state.deleted = FALSE;
  self->file_state.deleted_eof = FALSE;
  return file_reader_init_method(s);
}

static gboolean
_deinit(LogPipe *s)
{
  WildcardFileReader *self = (WildcardFileReader *)s;
  if (iv_task_registered(&self->file_state_event_handler))
    {
      iv_task_unregister(&self->file_state_event_handler);
    }
  return file_reader_deinit_method(s);
}

static void
_deleted_file_eof(FileStateEvent *self, FileReader *reader)
{
  if (self && self->deleted_file_eof)
    {
      self->deleted_file_eof(reader, self->deleted_file_eof_user_data);
    }
}

static void
_schedule_state_change_handling(WildcardFileReader *self)
{
  main_loop_assert_main_thread();
  if (!iv_task_registered(&self->file_state_event_handler))
    {
      iv_task_register(&self->file_state_event_handler);
    }
}

static void
_set_eof(WildcardFileReader *self)
{
  if (self->file_state.deleted)
    {
      self->file_state.deleted_eof = TRUE;
      _schedule_state_change_handling(self);
    }
}

static gboolean
_is_reader_poll_stopped(LogReader *reader)
{
  return (reader == NULL || FALSE == log_reader_is_opened(reader));
}

static void
_set_deleted(WildcardFileReader *self)
{
  /* File can be deleted only once,
   * so there is no need for checking the state
   * before we set it
   */
  self->file_state.deleted = TRUE;

  if (_is_reader_poll_stopped(self->super.reader))
    {
      self->file_state.deleted_eof = TRUE;
      _schedule_state_change_handling(self);
    }
}

static gboolean
_is_deleted_file_eof(WildcardFileReader *self)
{
  return (self->file_state.deleted && self->file_state.deleted_eof);
}

static gint
_notify(LogPipe *s, gint notify_code, gpointer user_data)
{
  WildcardFileReader *self = (WildcardFileReader *)s;
  gint result = NR_OK;
  switch(notify_code)
    {
    case NC_FILE_DELETED:
      _set_deleted(self);
      break;
    case NC_FILE_EOF:
      _set_eof(self);
      break;
    default:
      break;
    }
  result = file_reader_notify_method(s, notify_code, user_data);

  if (_is_deleted_file_eof(self))
    result |= NR_STOP_ON_EOF;
  return result;
}

static void
_handle_file_state_event(gpointer s)
{
  WildcardFileReader *self = (WildcardFileReader *)s;
  msg_debug("File status changed",
            evt_tag_int("EOF", self->file_state.deleted_eof),
            evt_tag_int("DELETED", self->file_state.deleted),
            evt_tag_str("Filename", self->super.filename->str));
  if (_is_deleted_file_eof(self))
    _deleted_file_eof(&self->file_state_event, &self->super);
}

static void
_on_file_moved(FileReader *self)
{
  log_pipe_notify(&self->super, NC_FILE_DELETED, self);
  log_pipe_notify(&self->super, NC_FILE_EOF, self);
}

void
wildcard_file_reader_on_deleted_file_eof(WildcardFileReader *self,
                                         FileStateEventCallback cb,
                                         gpointer user_data)
{
  self->file_state_event.deleted_file_eof = cb;
  self->file_state_event.deleted_file_eof_user_data = user_data;
}

gboolean
wildcard_file_reader_is_deleted(WildcardFileReader *self)
{
  return self->file_state.deleted;
}

WildcardFileReader *
wildcard_file_reader_new(const gchar *filename, FileReaderOptions *options, FileOpener *opener, LogSrcDriver *owner,
                         GlobalConfig *cfg, gboolean monitor_can_notify_file_changes)
{
  WildcardFileReader *self = g_new0(WildcardFileReader, 1);
  file_reader_init_instance(&self->super, filename, options, opener, owner, cfg, "wildcard_file_sd");
  self->super.super.init = _init;
  self->super.super.notify = _notify;
  self->super.super.deinit = _deinit;
  self->super.on_file_moved = _on_file_moved;
  IV_TASK_INIT(&self->file_state_event_handler);
  self->file_state_event_handler.cookie = self;
  self->file_state_event_handler.handler = _handle_file_state_event;
  self->super.monitor_can_notify_file_changes = monitor_can_notify_file_changes;
  return self;
}
