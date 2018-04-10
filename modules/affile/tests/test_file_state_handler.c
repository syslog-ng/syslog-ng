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

#include <criterion/criterion.h>
#include <glib/gstdio.h>
#include <unistd.h>
#include "apphook.h"
#include "file-state-handler.h"
#include "logpipe.h"
#include "iv.h"

#define TEST_FILE_NAME "TEST_FILE"

typedef struct _TestDeletedFileStateEvent
{
  DeletedFileStateEvent super;
  gboolean deleted_eof_called;
  gboolean finished_called;
} TestDeletedFileStateEvent;

static void
_eof(FileStateHandler *state_handler, gpointer user_data)
{
  TestDeletedFileStateEvent *test = (TestDeletedFileStateEvent *) user_data;
  test->deleted_eof_called = TRUE;
}

static void
_finished(FileStateHandler *state_handler, gpointer user_data)
{
  TestDeletedFileStateEvent *test = (TestDeletedFileStateEvent *) user_data;
  test->finished_called = TRUE;
}

TestDeletedFileStateEvent *
test_deleted_file_state_event_new(void)
{
  TestDeletedFileStateEvent *self = g_new0(TestDeletedFileStateEvent, 1);
  self->super.user_data = self;
  self->super.deleted_file_eof = _eof;
  self->super.deleted_file_finised = _finished;
  return self;
}

TestDeletedFileStateEvent *test_event;
FileStateHandler *test_handler;

static void
_init(void)
{
  app_startup();
  test_event = test_deleted_file_state_event_new();
  test_handler = file_state_handler_new(TEST_FILE_NAME, &test_event->super);
  cr_assert_eq(file_state_handler_init(test_handler), TRUE);
}

static void
_teardown(void)
{
  file_state_handler_free(test_handler);
  free(test_event);
  app_shutdown();
}

/* redefined iv_event_post just for testing */
void iv_event_post(struct iv_event *this)
{
  this->handler(this->cookie);
}

TestSuite(file_state_handler, .init = _init, .fini = _teardown);

Test(file_state_handler, constructor)
{
  cr_assert_eq(test_handler->last_msg_sent,TRUE);
  cr_assert_eq(test_handler->eof, FALSE);
  cr_assert_eq(test_handler->deleted, FALSE);
  cr_assert_str_eq(test_handler->filename, TEST_FILE_NAME);
}

Test(file_state_handler, msg_read)
{
  test_handler->eof = TRUE;
  file_state_handler_msg_read(test_handler);
  cr_assert_eq(test_handler->eof, FALSE);
  cr_assert_eq(test_handler->last_msg_sent, FALSE);
}

Test(file_state_handler, notif_deleted)
{
  file_state_handler_msg_read(test_handler);
  file_state_handler_notify(test_handler, NC_FILE_DELETED);
  cr_assert_eq(test_handler->deleted, TRUE);
}

Test(file_state_handler, notif_eof)
{
  file_state_handler_msg_read(test_handler);
  file_state_handler_notify(test_handler, NC_FILE_EOF);
  cr_assert_eq(test_handler->eof, TRUE);
}

Test(file_state_handler, notif_last_msg_sent)
{
  file_state_handler_msg_read(test_handler);
  file_state_handler_notify(test_handler, NC_LAST_MSG_SENT);
  cr_assert_eq(test_handler->last_msg_sent, TRUE);
}

Test(file_state_handler, status_change_deleted_not_eof_not_sent)
{
  file_state_handler_msg_read(test_handler);
  file_state_handler_notify(test_handler, NC_FILE_DELETED);
  cr_assert_eq(test_event->deleted_eof_called, FALSE);
  cr_assert_eq(test_event->finished_called, FALSE);
}

Test(file_state_handler, status_change_deleted_eof_not_sent)
{
  file_state_handler_msg_read(test_handler);
  file_state_handler_notify(test_handler, NC_FILE_DELETED);
  file_state_handler_notify(test_handler, NC_FILE_EOF);
  cr_assert_eq(test_event->deleted_eof_called, TRUE);
  cr_assert_eq(test_event->finished_called, FALSE);
}

Test(file_state_handler, status_change_deleted_eof_sent)
{
  file_state_handler_msg_read(test_handler);
  file_state_handler_notify(test_handler, NC_FILE_DELETED);
  file_state_handler_notify(test_handler, NC_FILE_EOF);
  file_state_handler_notify(test_handler, NC_LAST_MSG_SENT);
  cr_assert_eq(test_event->deleted_eof_called, TRUE);
  cr_assert_eq(test_event->finished_called, TRUE);
}

Test(file_state_handler, status_finished_then_delete)
{
  file_state_handler_msg_read(test_handler);
  file_state_handler_notify(test_handler, NC_FILE_EOF);
  file_state_handler_notify(test_handler, NC_LAST_MSG_SENT);
  cr_assert_eq(test_event->deleted_eof_called, FALSE);
  cr_assert_eq(test_event->finished_called, FALSE);

  file_state_handler_notify(test_handler, NC_FILE_DELETED);
  cr_assert_eq(test_event->deleted_eof_called, TRUE);
  cr_assert_eq(test_event->finished_called, TRUE);
}
