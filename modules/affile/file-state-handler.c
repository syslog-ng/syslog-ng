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

#include "file-state-handler.h"
#include "logpipe.h"

static void
_set_eof(FileStateHandler *self)
{
  self->eof = TRUE;
  if (self->deleted && self->eof)
    {
      iv_event_post(&self->deleted_file_status_changed);
    }
}

static void
_set_last_msg_sent(FileStateHandler *self)
{
  self->last_msg_sent = TRUE;
  if (self->deleted && self->last_msg_sent)
    {
      iv_event_post(&self->deleted_file_status_changed);
    }
}

static void
_set_deleted(FileStateHandler *self)
{
  /* File can be deleted only once,
   * so there is no need for checking the state
   * before we set it
   */
  self->deleted = TRUE;
  iv_event_post(&self->deleted_file_status_changed);
}

void
file_state_handler_msg_read(FileStateHandler *self)
{
  self->eof = FALSE;
  self->last_msg_sent = FALSE;
}

void
file_state_handler_notify(FileStateHandler *self, gint notify_code)
{
  switch (notify_code)
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
      break;
    }
}

gboolean
file_state_handler_init(FileStateHandler *self)
{
  self->deleted = FALSE;
  self->eof = FALSE;
  self->last_msg_sent = TRUE;
  return !iv_event_register(&self->deleted_file_status_changed);
}

void
file_state_handler_deinit(FileStateHandler *self)
{
  iv_event_unregister(&self->deleted_file_status_changed);
}

void file_state_handler_free(FileStateHandler *self)
{
  g_free(self);
}

static void
_deleted_file_finished(FileStateHandler *self)
{
  if (self->deleted_events && self->deleted_events->deleted_file_finised)
    {
      self->deleted_events->deleted_file_finised(self, self->deleted_events->user_data);
    }
}

static void
_deleted_file_eof(FileStateHandler *self)
{
  if (self->deleted_events && self->deleted_events->deleted_file_eof)
    {
      self->deleted_events->deleted_file_eof(self, self->deleted_events->user_data);
    }
}

static void
_deleted_file_status_changed(gpointer s)
{
  FileStateHandler *self = (FileStateHandler *) s;
  msg_debug("File status changed",
            evt_tag_int("EOF", self->eof),
            evt_tag_int("DELETED", self->deleted),
            evt_tag_int("LAST_MSG_SENT", self->last_msg_sent),
            evt_tag_str("Filename", self->filename));
  if (self->deleted && self->eof)
    {
      _deleted_file_eof(self);
      if (self->last_msg_sent)
        {
          _deleted_file_finished(self);
        }
    }
}

FileStateHandler *
file_state_handler_new(const gchar *filename,
                       DeletedFileStateEvent *deleted_events)
{
  FileStateHandler *self = g_new0(FileStateHandler, 1);

  self->last_msg_sent = TRUE;
  self->filename = filename;
  self->deleted_events = deleted_events;

  IV_EVENT_INIT(&self->deleted_file_status_changed);
  self->deleted_file_status_changed.cookie = self;
  self->deleted_file_status_changed.handler = _deleted_file_status_changed;
  return self;
}
