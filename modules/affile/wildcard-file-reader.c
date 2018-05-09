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
  self->file_state.eof = FALSE;
  self->file_state.last_msg_sent = TRUE;
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
_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  WildcardFileReader *self = (WildcardFileReader *)s;
  self->file_state.eof = FALSE;
  self->file_state.last_msg_sent = FALSE;
  file_reader_queue_method(s, msg, path_options);
}

static void
_deleted_file_finished(FileStateEvent *self, FileReader *reader)
{
  if (self && self->deleted_file_finised)
    {
      self->deleted_file_finised(reader, self->user_data);
    }
}

static void
_deleted_file_eof(FileStateEvent *self, FileReader *reader)
{
  if (self && self->deleted_file_eof)
    {
      self->deleted_file_eof(reader, self->user_data);
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
  self->file_state.eof = TRUE;
  if (self->file_state.deleted)
    {
      _schedule_state_change_handling(self);
    }
}

static void
_set_last_msg_sent(WildcardFileReader *self)
{
  self->file_state.last_msg_sent = TRUE;
  if (self->file_state.deleted)
    {
      _schedule_state_change_handling(self);
    }
}

static void
_set_deleted(WildcardFileReader *self)
{
  /* File can be deleted only once,
   * so there is no need for checking the state
   * before we set it
   */
  self->file_state.deleted = TRUE;
  _schedule_state_change_handling(self);
}

static void
_notify(LogPipe *s, gint notify_code, gpointer user_data)
{
  WildcardFileReader *self = (WildcardFileReader *)s;
  switch(notify_code)
    {
    case NC_FILE_DELETED:
      _set_deleted(self);
      break;
    case NC_FILE_EOF:
      _set_eof(self);
      break;
    case NC_LAST_MSG_SENT:
      _set_last_msg_sent(self);
      break;
    default:
      file_reader_notify_method(s, notify_code, user_data);
      break;
    }
}

static void
_handle_file_state_event(gpointer s)
{
  WildcardFileReader *self = (WildcardFileReader *)s;
  msg_debug("File status changed",
            evt_tag_int("EOF", self->file_state.eof),
            evt_tag_int("DELETED", self->file_state.deleted),
            evt_tag_int("LAST_MSG_SENT", self->file_state.last_msg_sent),
            evt_tag_str("Filename", self->super.filename->str));
  if (self->file_state.deleted && self->file_state.eof)
    {
      _deleted_file_eof(self->file_state_event, &self->super);
      if (self->file_state.last_msg_sent)
        {
          _deleted_file_finished(self->file_state_event, &self->super);
        }
    }
}

FileReader *
wildcard_file_reader_new(const gchar *filename, FileReaderOptions *options, FileOpener *opener, LogSrcDriver *owner,
                         GlobalConfig *cfg, FileStateEvent *deleted_file_events)
{
  WildcardFileReader *self = g_new0(WildcardFileReader, 1);
  file_reader_init_instance(&self->super, filename, options, opener, owner, cfg);
  self->super.super.init = _init;
  self->super.super.queue = _queue;
  self->super.super.notify = _notify;
  self->super.super.deinit = _deinit;
  self->file_state_event = deleted_file_events;
  IV_TASK_INIT(&self->file_state_event_handler);
  self->file_state_event_handler.cookie = self;
  self->file_state_event_handler.handler = _handle_file_state_event;
  return &self->super;
}
