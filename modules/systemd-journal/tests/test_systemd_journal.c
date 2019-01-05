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

#include "journald-mock.h"
#include "test-source.h"
#include "journald-helper.c"
#include "journal-reader.c"
#include "testutils.h"
#include "apphook.h"

#define JOURNALD_TESTCASE(testfunc, ...) { testcase_begin("%s(%s)", #testfunc, #__VA_ARGS__); testfunc(__VA_ARGS__); testcase_end(); }

#define TEST_PERSIST_FILE_NAME "test_systemd_journal.persist"

static gboolean task_called;
static gboolean poll_triggered;

void
add_mock_entries(gpointer user_data)
{
  Journald *journald = user_data;
  MockEntry *entry = mock_entry_new("test_data3");
  mock_entry_add_data(entry, "MESSAGE=test message3");
  mock_entry_add_data(entry, "KEY=VALUE3");
  mock_entry_add_data(entry, "HOST=testhost3");
  journald_mock_add_entry(journald, entry);

  entry = mock_entry_new("test_data4");
  mock_entry_add_data(entry, "MESSAGE=test message4");
  mock_entry_add_data(entry, "KEY=VALUE4");
  mock_entry_add_data(entry, "HOST=testhost4");
  journald_mock_add_entry(journald, entry);
}

void
handle_new_entry(gpointer user_data)
{
  Journald *journald = user_data;
  journald_process(journald);
  assert_false(poll_triggered, ASSERTION_ERROR("Should called only once"));
  poll_triggered = TRUE;
}

void
stop_timer_expired(gpointer user_data)
{
  iv_quit();
}

void
__test_seeks(Journald *journald)
{
  gint result = journald_seek_head(journald);
  assert_gint(result, 0, ASSERTION_ERROR("Can't seek in empty journald mock"));
  result = journald_next(journald);
  assert_gint(result, 0, ASSERTION_ERROR("Bad next step result"));

  MockEntry *entry = mock_entry_new("test_data1");
  mock_entry_add_data(entry, "MESSAGE=test message");
  mock_entry_add_data(entry, "KEY=VALUE");
  mock_entry_add_data(entry, "HOST=testhost");

  journald_mock_add_entry(journald, entry);

  entry = mock_entry_new("test_data2");
  mock_entry_add_data(entry, "MESSAGE=test message2");
  mock_entry_add_data(entry, "KEY=VALUE2");
  mock_entry_add_data(entry, "HOST=testhost2");

  journald_mock_add_entry(journald, entry);

  result = journald_seek_head(journald);
  assert_gint(result, 0, ASSERTION_ERROR("Can't seek in journald mock"));
  result = journald_next(journald);
  assert_gint(result, 1, ASSERTION_ERROR("Bad next step result"));

  result = journald_seek_tail(journald);
  assert_gint(result, 0, ASSERTION_ERROR("Can't seek in journald mock"));
  result = journald_next(journald);
  assert_gint(result, 1, ASSERTION_ERROR("Bad next step result"));
  result = journald_next(journald);
  assert_gint(result, 0, ASSERTION_ERROR("Bad next step result"));


}

void
__test_cursors(Journald *journald)
{
  gchar *cursor;
  journald_seek_head(journald);
  journald_next(journald);
  gint result = journald_get_cursor(journald, &cursor);
  assert_string(cursor, "test_data1", ASSERTION_ERROR("Bad cursor fetched"));
  \
  g_free(cursor);

  result = journald_next(journald);
  assert_gint(result, 1, ASSERTION_ERROR("Bad next step result"));
  result = journald_get_cursor(journald, &cursor);
  assert_string(cursor, "test_data2", ASSERTION_ERROR("Bad cursor fetched"));
  g_free(cursor);

  result = journald_next(journald);
  assert_gint(result, 0, ASSERTION_ERROR("Should not contain more elements"));

  result = journald_seek_cursor(journald, "test_data1");
  assert_gint(result, 0, ASSERTION_ERROR("Should find cursor"));
  result = journald_next(journald);
  assert_gint(result, 1, ASSERTION_ERROR("Bad next step result"));
  result = journald_get_cursor(journald, &cursor);
  assert_string(cursor, "test_data1", ASSERTION_ERROR("Bad cursor fetched"));
  g_free(cursor);

  result = journald_seek_cursor(journald, "test_data2");
  assert_gint(result, 0, ASSERTION_ERROR("Should find cursor"));
  result = journald_next(journald);
  assert_gint(result, 1, ASSERTION_ERROR("Bad next step result"));
  result = journald_get_cursor(journald, &cursor);
  assert_string(cursor, "test_data2", ASSERTION_ERROR("Bad cursor fetched"));
  g_free(cursor);
}

void
__test_enumerate(Journald *journald)
{
  const void *data;
  const void *prev_data;
  gsize length;
  gsize prev_len;
  gint result;

  journald_restart_data(journald);
  result = journald_enumerate_data(journald, &data, &length);
  assert_gint(result, 1, ASSERTION_ERROR("Data should exist"));

  prev_data = data;
  prev_len = length;

  result = journald_enumerate_data(journald, &data, &length);
  assert_gint(result, 1, ASSERTION_ERROR("Data should exist"));
  result = journald_enumerate_data(journald, &data, &length);
  assert_gint(result, 1, ASSERTION_ERROR("Data should exist"));
  result = journald_enumerate_data(journald, &data, &length);
  assert_gint(result, 0, ASSERTION_ERROR("Data should not exist"));

  journald_restart_data(journald);

  result = journald_enumerate_data(journald, &data, &length);
  assert_gint(result, 1, ASSERTION_ERROR("Data should exist"));
  assert_gpointer((gpointer )data, (gpointer )prev_data,
                  ASSERTION_ERROR("restart data should seek the start of the data"));
  assert_gint(length, prev_len, ASSERTION_ERROR("Bad length after restart data"));

  result = journald_next(journald);
  assert_gint(result, 0, ASSERTION_ERROR("Should not contain more elements"));
}

void
__test_fd_handling(Journald *journald)
{
  gint fd = journald_get_fd(journald);
  journald_process(journald);

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

  assert_true(poll_triggered, ASSERTION_ERROR("Poll event isn't triggered"));
}

void
test_journald_mock(void)
{
  Journald *journald = journald_mock_new();
  gint result = journald_open(journald, 0);

  assert_gint(result, 0, ASSERTION_ERROR("Can't open journald mock"));

  __test_seeks(journald);

  __test_cursors(journald);

  __test_fd_handling(journald);

  journald_close(journald);
  journald_free(journald);
}

void
__helper_test(gchar *key, gchar *value, gpointer user_data)
{
  GHashTable *result = user_data;
  g_hash_table_insert(result, g_strdup(key), g_strdup(value));
  return;
}


void
test_journald_helper(void)
{
  Journald *journald = journald_mock_new();
  journald_open(journald, 0);

  MockEntry *entry = mock_entry_new("test_data1");
  mock_entry_add_data(entry, "MESSAGE=test message");
  mock_entry_add_data(entry, "KEY=VALUE");
  mock_entry_add_data(entry, "HOST=testhost");

  journald_mock_add_entry(journald, entry);
  journald_seek_head(journald);
  journald_next(journald);

  GHashTable *result = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  journald_foreach_data(journald, __helper_test, result);

  gchar *message = g_hash_table_lookup(result, "MESSAGE");
  gchar *key = g_hash_table_lookup(result, "KEY");
  gchar *host = g_hash_table_lookup(result, "HOST");

  assert_string(message, "test message", ASSERTION_ERROR("Bad item"));
  assert_string(key, "VALUE", ASSERTION_ERROR("Bad item"));
  assert_string(host, "testhost", ASSERTION_ERROR("Bad item"));

  journald_close(journald);
  journald_free(journald);
  g_hash_table_unref(result);
}

MockEntry *
__create_real_entry(Journald *journal, gchar *cursor_name)
{
  MockEntry *entry = mock_entry_new(cursor_name);
  mock_entry_add_data(entry, "PRIORITY=6");
  mock_entry_add_data(entry, "_UID=0");
  mock_entry_add_data(entry, "_GID=0");
  mock_entry_add_data(entry, "_BOOT_ID=90129174f7d742e6b4d06eb846e94c87");
  mock_entry_add_data(entry, "_MACHINE_ID=d0dc49b7a2784e18b3689d9d9c35f47d");
  mock_entry_add_data(entry, "_HOSTNAME=localhost.localdomain");
  mock_entry_add_data(entry, "_CAP_EFFECTIVE=1fffffffff");
  mock_entry_add_data(entry, "_TRANSPORT=syslog");
  mock_entry_add_data(entry, "SYSLOG_FACILITY=10");
  mock_entry_add_data(entry, "SYSLOG_IDENTIFIER=sshd");
  mock_entry_add_data(entry, "_COMM=sshd");
  mock_entry_add_data(entry, "_EXE=/usr/sbin/sshd");
  mock_entry_add_data(entry, "_SELINUX_CONTEXT=system_u:system_r:sshd_t:s0-s0:c0.c1023");
  mock_entry_add_data(entry, "_AUDIT_LOGINUID=1000");
  mock_entry_add_data(entry, "_SYSTEMD_OWNER_UID=1000");
  mock_entry_add_data(entry, "_SYSTEMD_SLICE=user-1000.slice");
  mock_entry_add_data(entry, "SYSLOG_PID=2240");
  mock_entry_add_data(entry, "_PID=2240");
  mock_entry_add_data(entry, "_CMDLINE=sshd: foo_user [priv]");
  mock_entry_add_data(entry, "MESSAGE=pam_unix(sshd:session): session opened for user foo_user by (uid=0)");
  mock_entry_add_data(entry, "_AUDIT_SESSION=2");
  mock_entry_add_data(entry, "_SYSTEMD_CGROUP=/user.slice/user-1000.slice/session-2.scope");
  mock_entry_add_data(entry, "_SYSTEMD_SESSION=2");
  mock_entry_add_data(entry, "_SYSTEMD_UNIT=session-2.scope");
  mock_entry_add_data(entry, "_SOURCE_REALTIME_TIMESTAMP=1408967385496986");

  return entry;
}

MockEntry *
__create_dummy_entry(Journald *journal, gchar *cursor_name)
{
  MockEntry *entry = mock_entry_new(cursor_name);
  mock_entry_add_data(entry, "MESSAGE=Dummy message");
  mock_entry_add_data(entry, "_PID=2345");
  mock_entry_add_data(entry, "_COMM=dummy");
  return entry;
}

void
_test_default_working_init(TestCase *self, TestSource *src, Journald *journal, JournalReader *reader,
                           JournalReaderOptions *options)
{
  MockEntry *entry = __create_dummy_entry(journal, "default_test");
  journald_mock_add_entry(journal, entry);
  configuration->recv_time_zone = g_strdup("+06:00");
  self->user_data = options;
}

void
_test_default_working_test(TestCase *self, TestSource *src, LogMessage *msg)
{
  const gchar *message = log_msg_get_value(msg, LM_V_MESSAGE, NULL);
  JournalReaderOptions *options = self->user_data;
  assert_string(message, "Dummy message", ASSERTION_ERROR("Bad message"));
  assert_gint(msg->pri, options->default_pri, ASSERTION_ERROR("Bad default prio"));
  assert_gint(options->fetch_limit, 10, ASSERTION_ERROR("Bad default fetch_limit"));
  assert_gint(options->max_field_size, 64 * 1024, ASSERTION_ERROR("Bad max field size"));
  assert_string(options->prefix, ".journald.", ASSERTION_ERROR("Bad default prefix value"));
  assert_string(options->recv_time_zone, configuration->recv_time_zone, ASSERTION_ERROR("Bad default timezone"));
  test_source_finish_tc(src);
}

void
_test_prefix_init(TestCase *self, TestSource *src, Journald *journal, JournalReader *reader,
                  JournalReaderOptions *options)
{
  MockEntry *entry = __create_real_entry(journal, "prefix_test");

  options->prefix = g_strdup((gchar *)self->user_data);

  journald_mock_add_entry(journal, entry);
}

void
__test_other_has_prefix(TestCase *self, LogMessage *msg)
{
  gchar *requested_name = g_strdup_printf("%s%s", (gchar *) self->user_data, "_CMDLINE");
  gssize value_len;
  const gchar *value = log_msg_get_value_by_name(msg, requested_name, &value_len);
  assert_string(value, "sshd: foo_user [priv]", ASSERTION_ERROR("Bad value for prefixed key"));
  g_free(requested_name);
}

void
_test_prefix_test(TestCase *self, TestSource *src, LogMessage *msg)
{
  const gchar *message = log_msg_get_value(msg, LM_V_MESSAGE, NULL);
  assert_string(message, "pam_unix(sshd:session): session opened for user foo_user by (uid=0)",
                ASSERTION_ERROR("Bad message"));

  __test_other_has_prefix(self, msg);

  test_source_finish_tc(src);
}

void
_test_field_size_init(TestCase *self, TestSource *src, Journald *journal, JournalReader *reader,
                      JournalReaderOptions *options)
{
  MockEntry *entry = __create_real_entry(journal, "field_size_test");
  options->max_field_size = GPOINTER_TO_INT(self->user_data);
  journald_mock_add_entry(journal, entry);
}

gboolean
__check_value_len(NVHandle handle, const gchar *name, const gchar *value, gssize value_len, gpointer user_data)
{
  TestCase *self = user_data;
  gchar *error_message = g_strdup_printf("Bad value size; name: %s, value: %s len: %ld", name, value, value_len);
  if (strcmp(name, "HOST_FROM") != 0)
    {
      assert_true(value_len <= GPOINTER_TO_INT(self->user_data), ASSERTION_ERROR(error_message));
    }
  g_free(error_message);
  return FALSE;
}


void
_test_field_size_test(TestCase *self, TestSource *src, LogMessage *msg)
{
  log_msg_values_foreach(msg, __check_value_len, self);
  test_source_finish_tc(src);
}

void
_test_timezone_init(TestCase *self, TestSource *src, Journald *journal, JournalReader *reader,
                    JournalReaderOptions *options)
{
  MockEntry *entry = __create_real_entry(journal, "time_zone_test");
  options->recv_time_zone = g_strdup("+09:00");
  journald_mock_add_entry(journal, entry);
}

void
_test_timezone_test(TestCase *self, TestSource *src, LogMessage *msg)
{
  assert_gint(msg->timestamps[LM_TS_STAMP].ut_gmtoff, 9 * 3600, ASSERTION_ERROR("Bad time zone info"));
  test_source_finish_tc(src);
}

void
_test_default_level_init(TestCase *self, TestSource *src, Journald *journal, JournalReader *reader,
                         JournalReaderOptions *options)
{
  MockEntry *entry = __create_dummy_entry(journal, "test default level");
  gint level = GPOINTER_TO_INT(self->user_data);
  if (options->default_pri == 0xFFFF)
    options->default_pri = LOG_USER;
  options->default_pri = (options->default_pri & ~7) | level;

  journald_mock_add_entry(journal, entry);
}

void
_test_default_level_test(TestCase *self, TestSource *src, LogMessage *msg)
{
  gint level = GPOINTER_TO_INT(self->user_data);
  assert_gint(msg->pri, LOG_LOCAL0 | level, ASSERTION_ERROR("Bad default prio"));
  test_source_finish_tc(src);
}

void
_test_default_facility_init(TestCase *self, TestSource *src, Journald *journal, JournalReader *reader,
                            JournalReaderOptions *options)
{
  MockEntry *entry = __create_dummy_entry(journal, "test default facility");
  gint facility = GPOINTER_TO_INT(self->user_data);
  if(options->default_pri == 0xFFFF)
    options->default_pri = LOG_NOTICE;
  options->default_pri = (options->default_pri & 7) | facility;
  journald_mock_add_entry(journal, entry);
}

void
_test_default_facility_test(TestCase *self, TestSource *src, LogMessage *msg)
{
  gint facility = GPOINTER_TO_INT(self->user_data);
  assert_gint(msg->pri, facility | LOG_NOTICE, ASSERTION_ERROR("Bad default prio"));
  test_source_finish_tc(src);
}

void
_test_program_field_init(TestCase *self, TestSource *src, Journald *journal, JournalReader *reader,
                         JournalReaderOptions *options)
{
  MockEntry *entry = mock_entry_new("test _COMM first win");
  mock_entry_add_data(entry, "_COMM=comm_program");
  mock_entry_add_data(entry, "SYSLOG_IDENTIFIER=syslog_program");
  journald_mock_add_entry(journal, entry);

  entry = mock_entry_new("test _COMM second win");
  mock_entry_add_data(entry, "SYSLOG_IDENTIFIER=syslog_program");
  mock_entry_add_data(entry, "_COMM=comm_program");
  journald_mock_add_entry(journal, entry);

  entry = mock_entry_new("no SYSLOG_IDENTIFIER");
  mock_entry_add_data(entry, "_COMM=comm_program");
  journald_mock_add_entry(journal, entry);

  self->user_data = journal;
}

void
_test_program_field_test(TestCase *self, TestSource *src, LogMessage *msg)
{
  Journald *journal = self->user_data;
  gchar *cursor;
  journald_get_cursor(journal, &cursor);
  if (strcmp(cursor, "no SYSLOG_IDENTIFIER") != 0)
    {
      assert_string(log_msg_get_value(msg, LM_V_PROGRAM, NULL), "syslog_program", ASSERTION_ERROR("Bad program name"));
      g_free(cursor);
    }
  else
    {
      assert_string(log_msg_get_value(msg, LM_V_PROGRAM, NULL), "comm_program", ASSERTION_ERROR("Bad program name"));
      g_free(cursor);
      test_source_finish_tc(src);
    }
}

void
test_journal_reader(void)
{
  TestSource *src = test_source_new(configuration);
  TestCase tc_default_working = { _test_default_working_init, _test_default_working_test, NULL, NULL };
  TestCase tc_prefix = { _test_prefix_init, _test_prefix_test, NULL, "this.is.a.prefix." };
  TestCase tc_max_field_size = { _test_field_size_init, _test_field_size_test, NULL, GINT_TO_POINTER(10)};
  TestCase tc_timezone = { _test_timezone_init, _test_timezone_test, NULL, NULL };
  TestCase tc_default_level =  { _test_default_level_init, _test_default_level_test, NULL, GINT_TO_POINTER(LOG_ERR) };
  TestCase tc_default_facility = { _test_default_facility_init, _test_default_facility_test, NULL, GINT_TO_POINTER(LOG_AUTH) };
  TestCase tc_program_field = { _test_program_field_init, _test_program_field_test, NULL, NULL };

  test_source_add_test_case(src, &tc_default_working);
  test_source_add_test_case(src, &tc_prefix);
  test_source_add_test_case(src, &tc_max_field_size);
  test_source_add_test_case(src, &tc_timezone);
  test_source_add_test_case(src, &tc_default_level);
  test_source_add_test_case(src, &tc_default_facility);
  test_source_add_test_case(src, &tc_program_field);

  test_source_run_tests(src);
  log_pipe_unref((LogPipe *)src);
}

int
main(int argc, char **argv)
{
  app_startup();
  main_thread_handle =  get_thread_id();
  configuration = cfg_new_snippet();
  configuration->threaded = FALSE;
  configuration->state = persist_state_new(TEST_PERSIST_FILE_NAME);
  configuration->keep_hostname = TRUE;
  persist_state_start(configuration->state);
  JOURNALD_TESTCASE(test_journald_mock);
  JOURNALD_TESTCASE(test_journald_helper);
  JOURNALD_TESTCASE(test_journal_reader);
  persist_state_cancel(configuration->state);
  unlink(TEST_PERSIST_FILE_NAME);
  app_shutdown();
  return 0;
}
