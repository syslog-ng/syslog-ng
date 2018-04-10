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

#ifndef MODULES_AFFILE_FILE_STATE_HANDLER_H_
#define MODULES_AFFILE_FILE_STATE_HANDLER_H_

#include "syslog-ng.h"
#include "iv_event.h"

typedef struct _FileStateHandler FileStateHandler;

typedef void (*FileStateEventCallback)(FileStateHandler *state_handler, gpointer user_data);

typedef struct _DeletedFileStateEvent
{
  FileStateEventCallback deleted_file_finised;
  FileStateEventCallback deleted_file_eof;
  gpointer user_data;
} DeletedFileStateEvent;

struct _FileStateHandler
{
  gboolean deleted;
  gboolean eof;
  gboolean last_msg_sent;
  DeletedFileStateEvent *deleted_events;
  struct iv_event deleted_file_status_changed;
  const gchar *filename;
};

FileStateHandler *file_state_handler_new(const gchar *filename,
                                         DeletedFileStateEvent *event);

void file_state_handler_msg_read(FileStateHandler *self);
void file_state_handler_notify(FileStateHandler *self, gint notify_code);

gboolean file_state_handler_init(FileStateHandler *self);
void file_state_handler_deinit(FileStateHandler *self);

void file_state_handler_free(FileStateHandler *self);


#endif /* MODULES_AFFILE_FILE_STATE_HANDLER_H_ */
