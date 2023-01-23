/*
 * Copyright (c) 2010-2014 Balabit
 * Copyright (c) 2010-2014 Viktor Juhasz <viktor.juhasz@balabit.com>
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

#include "journald-subsystem.h"
#include "journald-mock.h"

#include "apphook.h"
#include "mainloop.h"
#include "cfg.h"

#include <iv.h>

void
__test_seeks(sd_journal *journald)
{
  gint result = sd_journal_seek_head(journald);
  cr_assert_eq(result, 0, "%s", "Can't seek in empty journald mock");
  result = sd_journal_next(journald);
  cr_assert_eq(result, 0, "%s", "Bad next step result");

  MockEntry *entry = mock_entry_new("test_data1");
  mock_entry_add_data(entry, "MESSAGE=test message");
  mock_entry_add_data(entry, "KEY=VALUE");
  mock_entry_add_data(entry, "HOST=testhost");

  mock_journal_add_entry(entry);

  entry = mock_entry_new("test_data2");
  mock_entry_add_data(entry, "MESSAGE=test message2");
  mock_entry_add_data(entry, "KEY=VALUE2");
  mock_entry_add_data(entry, "HOST=testhost2");

  mock_journal_add_entry(entry);

  result = sd_journal_seek_head(journald);
  cr_assert_eq(result, 0, "%s", "Can't seek in journald mock");
  result = sd_journal_next(journald);
  cr_assert_eq(result, 1, "%s", "Bad next step result");

  result = sd_journal_seek_tail(journald);
  cr_assert_eq(result, 0, "%s", "Can't seek in journald mock");
  result = sd_journal_next(journald);
  cr_assert_eq(result, 1, "%s", "Bad next step result");
  result = sd_journal_next(journald);
  cr_assert_eq(result, 0, "%s", "Bad next step result");
}

void
__test_cursors(sd_journal *journald)
{
  gchar *cursor;
  sd_journal_seek_head(journald);
  sd_journal_next(journald);
  gint result = sd_journal_get_cursor(journald, &cursor);
  cr_assert_str_eq(cursor, "test_data1", "%s", "Bad cursor fetched");
  \
  g_free(cursor);

  result = sd_journal_next(journald);
  cr_assert_eq(result, 1, "%s", "Bad next step result");
  result = sd_journal_get_cursor(journald, &cursor);
  cr_assert_str_eq(cursor, "test_data2", "%s", "Bad cursor fetched");
  g_free(cursor);

  result = sd_journal_next(journald);
  cr_assert_eq(result, 0, "%s", "Should not contain more elements");

  result = sd_journal_seek_cursor(journald, "test_data1");
  cr_assert_eq(result, 0, "%s", "Should find cursor");
  result = sd_journal_next(journald);
  cr_assert_eq(result, 1, "%s", "Bad next step result");
  result = sd_journal_get_cursor(journald, &cursor);
  cr_assert_str_eq(cursor, "test_data1", "%s", "Bad cursor fetched");
  g_free(cursor);

  result = sd_journal_seek_cursor(journald, "test_data2");
  cr_assert_eq(result, 0, "%s", "Should find cursor");
  result = sd_journal_next(journald);
  cr_assert_eq(result, 1, "%s", "Bad next step result");
  result = sd_journal_get_cursor(journald, &cursor);
  cr_assert_str_eq(cursor, "test_data2", "%s", "Bad cursor fetched");
  g_free(cursor);
}

static gboolean poll_triggered;
static gboolean task_called;

void
add_mock_entries(gpointer user_data)
{
  MockEntry *entry = mock_entry_new("test_data3");
  mock_entry_add_data(entry, "MESSAGE=test message3");
  mock_entry_add_data(entry, "KEY=VALUE3");
  mock_entry_add_data(entry, "HOST=testhost3");
  mock_journal_add_entry(entry);

  entry = mock_entry_new("test_data4");
  mock_entry_add_data(entry, "MESSAGE=test message4");
  mock_entry_add_data(entry, "KEY=VALUE4");
  mock_entry_add_data(entry, "HOST=testhost4");
  mock_journal_add_entry(entry);
}

void
handle_new_entry(gpointer user_data)
{
  sd_journal *journald = user_data;
  sd_journal_process(journald);
  cr_assert_not(poll_triggered, "%s", "Should called only once");
  poll_triggered = TRUE;
}

void
stop_timer_expired(gpointer user_data)
{
  iv_quit();
}

void
__test_fd_handling(sd_journal *journald)
{
  gint fd = sd_journal_get_fd(journald);
  sd_journal_process(journald);

  task_called = FALSE;
  poll_triggered = FALSE;
  struct iv_task add_entry_task;
  struct iv_fd fd_to_poll;
  struct iv_timer stop_timer;

  IV_TASK_INIT(&add_entry_task);
  add_entry_task.cookie = journald;
  add_entry_task.handler = add_mock_entries;

  IV_FD_INIT(&fd_to_poll);
  fd_to_poll.fd = fd;
  fd_to_poll.cookie = journald;
  fd_to_poll.handler_in = handle_new_entry;

  iv_validate_now();
  IV_TIMER_INIT(&stop_timer);
  stop_timer.cookie = NULL;
  stop_timer.expires = iv_now;
  stop_timer.expires.tv_sec++;
  stop_timer.handler = stop_timer_expired;

  iv_task_register(&add_entry_task);
  iv_fd_register(&fd_to_poll);
  iv_timer_register(&stop_timer);

  iv_main();

  cr_assert(poll_triggered, "%s", "Poll event isn't triggered");
}

Test(journald_mock, test_journald_mock)
{
  sd_journal *journald;
  gint result = sd_journal_open(&journald, 0);

  cr_assert_eq(result, 0, "%s", "Can't open journald mock");

  __test_seeks(journald);

  __test_cursors(journald);

  __test_fd_handling(journald);

  sd_journal_close(journald);
}

#define PERSIST_FILE "test_journald_mock.persist"

static void
setup(void)
{
  app_startup();

  main_thread_handle = get_thread_id();
  configuration = cfg_new_snippet();
  configuration->threaded = FALSE;
  configuration->state = persist_state_new(PERSIST_FILE);
  configuration->keep_hostname = TRUE;
  persist_state_start(configuration->state);
}

static void
teardown(void)
{
  persist_state_cancel(configuration->state);
  unlink(PERSIST_FILE);
  cfg_free(configuration);
  app_shutdown();
}

TestSuite(journald_mock, .init = setup, .fini = teardown);
