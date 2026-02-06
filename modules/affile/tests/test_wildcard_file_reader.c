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

#include "apphook.h"
#include "wildcard-file-reader.h"
#include "logpipe.h"
#include "poll-file-changes.h"

#include "iv.h"
#include <glib/gstdio.h>
#include <unistd.h>

#define TEST_FILE_NAME "TEST_FILE"


typedef struct _TestFileStateEvent
{
  gboolean deleted_eof_called;
  gboolean finished_called;
} TestFileStateEvent;

static void
_eof(FileReader *reader, gpointer user_data)
{
  TestFileStateEvent *test = (TestFileStateEvent *) user_data;

  cr_assert_eq(test->deleted_eof_called, FALSE);

  test->deleted_eof_called = TRUE;
}

TestFileStateEvent *
test_deleted_file_state_event_new(void)
{
  TestFileStateEvent *self = g_new0(TestFileStateEvent, 1);
  return self;
}

TestFileStateEvent *test_event;
WildcardFileReader *reader;
LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

/* MOCKS */

gboolean file_reader_init_method(LogPipe *s)
{
  return TRUE;
}

gboolean file_reader_deinit_method(LogPipe *s)
{
  return TRUE;
}

void file_reader_queue_method(LogPipe *s, LogMessage *msg, const LogPathOptions *po)
{
  return;
}

gint file_reader_notify_method(LogPipe *s, gint notify_code, gpointer user_data)
{
  return NR_OK;
}

gboolean
log_reader_is_opened(LogReader *self)
{
  return TRUE;
}

#if SYSLOG_NG_USE_CONST_IVYKIS_MOCK
int iv_task_registered(const struct iv_task *_t)
#else
int iv_task_registered(struct iv_task *_t)
#endif
{
  return 0;
}

void iv_task_register(struct iv_task *_t)
{
  _t->handler(_t->cookie);
}

void file_reader_init_instance(FileReader *self, const gchar *filename,
                               FileReaderOptions *options, FileOpener *opener,
                               LogSrcDriver *owner, GlobalConfig *cfg,
                               const gchar *persist_name_prefix)
{
  log_pipe_init_instance(&self->super, cfg);
  self->reader = log_reader_new(cfg);
  self->reader->poll_events = poll_file_changes_new(-1, "", 1, self);
  return;
}


static void
_init(void)
{
  app_startup();
  configuration = cfg_new_snippet();
  test_event = test_deleted_file_state_event_new();
  reader = (WildcardFileReader *)wildcard_file_reader_new(TEST_FILE_NAME, NULL, NULL, NULL, configuration, FALSE);
  wildcard_file_reader_on_deleted_file_eof(reader, _eof, test_event);
  cr_assert_eq(log_pipe_init(&reader->super.super), TRUE);
}

static void
_teardown(void)
{
  log_pipe_deinit(&reader->super.super);
  log_pipe_unref(&reader->super.super);
  free(test_event);
  if (configuration)
    cfg_free(configuration);
  app_shutdown();
}

TestSuite(test_wildcard_file_reader, .init = _init, .fini = _teardown);

Test(test_wildcard_file_reader, constructor)
{
  cr_assert_eq(reader->file_state.deleted_eof, FALSE);
  cr_assert_eq(reader->file_state.deleted, FALSE);
}

Test(test_wildcard_file_reader, notif_deleted)
{
  log_pipe_queue(&reader->super.super, NULL, &path_options);
  log_pipe_notify(&reader->super.super, NC_FILE_DELETED, NULL);
  cr_assert_eq(reader->file_state.deleted, TRUE);
  cr_assert_eq(reader->file_state.deleted_eof, FALSE);
}

Test(test_wildcard_file_reader, notif_eof)
{
  log_pipe_queue(&reader->super.super, NULL, &path_options);
  log_pipe_notify(&reader->super.super, NC_FILE_EOF, NULL);
  cr_assert_eq(reader->file_state.deleted_eof, FALSE);
}

Test(test_wildcard_file_reader, notif_eof_after_deleted)
{
  log_pipe_queue(&reader->super.super, NULL, &path_options);

  cr_assert_eq(reader->file_state.deleted_eof, FALSE);
  cr_assert_eq(reader->file_state.deleted, FALSE);

  gint result = log_pipe_notify(&reader->super.super, NC_FILE_DELETED, NULL);
  cr_assert_eq(result & NR_STOP_ON_EOF, 0);

  result = log_pipe_notify(&reader->super.super, NC_FILE_EOF, NULL);
  cr_assert_eq(result & NR_STOP_ON_EOF, NR_STOP_ON_EOF);
  cr_assert_eq(reader->file_state.deleted_eof, TRUE);
}

Test(test_wildcard_file_reader, status_change_deleted_eof)
{
  log_pipe_queue(&reader->super.super, NULL, &path_options);

  cr_assert_eq(reader->file_state.deleted_eof, FALSE);
  cr_assert_eq(reader->file_state.deleted, FALSE);

  log_pipe_notify(&reader->super.super, NC_FILE_EOF, NULL);
  cr_assert_eq(test_event->deleted_eof_called, FALSE);

  log_pipe_notify(&reader->super.super, NC_FILE_DELETED, NULL);
  cr_assert_eq(test_event->deleted_eof_called, FALSE);

  log_pipe_notify(&reader->super.super, NC_FILE_EOF, NULL);
  cr_assert_eq(test_event->deleted_eof_called, TRUE);
}
