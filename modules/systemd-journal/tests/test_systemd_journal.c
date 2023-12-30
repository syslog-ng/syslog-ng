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
#include "test-source.h"
#include "journald-mock.h"

#include "journald-helper.c"
#include "journal-reader.c"
#include "apphook.h"

static GlobalConfig *cfg;

static void
_init_cfg_with_persist_file(const gchar *persist_file)
{
  main_thread_handle = get_thread_id();
  cfg = cfg_new_snippet();
  cfg->threaded = FALSE;
  cfg->state = persist_state_new(persist_file);
  cfg->keep_hostname = TRUE;
  persist_state_start(cfg->state);
}

static void
_deinit_cfg(const gchar *persist_file)
{
  persist_state_cancel(cfg->state);
  unlink(persist_file);
  cfg_free(cfg);
}

void
__test_enumerate(sd_journal *journal)
{
  const void *data;
  const void *prev_data;
  gsize length;
  gsize prev_len;
  gint result;

  sd_journal_restart_data(journal);
  result = sd_journal_enumerate_data(journal, &data, &length);
  cr_assert_eq(result, 1, "%s", "Data should exist");

  prev_data = data;
  prev_len = length;

  result = sd_journal_enumerate_data(journal, &data, &length);
  cr_assert_eq(result, 1, "%s", "Data should exist");
  result = sd_journal_enumerate_data(journal, &data, &length);
  cr_assert_eq(result, 1, "%s", "Data should exist");
  result = sd_journal_enumerate_data(journal, &data, &length);
  cr_assert_eq(result, 0, "%s", "Data should not exist");

  sd_journal_restart_data(journal);

  result = sd_journal_enumerate_data(journal, &data, &length);
  cr_assert_eq(result, 1, "%s", "Data should exist");
  cr_assert_eq((gpointer )data, (gpointer )prev_data,
               "%s", "restart data should seek the start of the data");
  cr_assert_eq(length, prev_len, "%s", "Bad length after restart data");

  result = sd_journal_next(journal);
  cr_assert_eq(result, 0, "%s", "Should not contain more elements");
}


void
__helper_test(gchar *key, gsize key_len, gchar *value, gsize value_len, gpointer user_data)
{
  GHashTable *result = user_data;
  g_hash_table_insert(result, g_strndup(key, key_len), g_strndup(value, value_len));
  return;
}


Test(systemd_journal, test_journald_helper)
{
  const gchar *persist_file = "test_systemd_journal2.persist";

  _init_cfg_with_persist_file(persist_file);
  sd_journal *journal;
  sd_journal_open(&journal, 0);

  MockEntry *entry = mock_entry_new("test_data1");
  mock_entry_add_data(entry, "MESSAGE=test message");
  mock_entry_add_data(entry, "KEY=VALUE");
  mock_entry_add_data(entry, "HOST=testhost");

  mock_journal_add_entry(entry);
  sd_journal_seek_head(journal);
  sd_journal_next(journal);

  GHashTable *result = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  journald_foreach_data(journal, __helper_test, result);

  gchar *message = g_hash_table_lookup(result, "MESSAGE");
  gchar *key = g_hash_table_lookup(result, "KEY");
  gchar *host = g_hash_table_lookup(result, "HOST");

  cr_assert_str_eq(message, "test message", "%s", "Bad item");
  cr_assert_str_eq(key, "VALUE", "%s", "Bad item");
  cr_assert_str_eq(host, "testhost", "%s", "Bad item");

  sd_journal_close(journal);
  g_hash_table_unref(result);

  _deinit_cfg(persist_file);
}

MockEntry *
__create_real_entry(gchar *cursor_name)
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
__create_dummy_entry(gchar *cursor_name)
{
  MockEntry *entry = mock_entry_new(cursor_name);
  mock_entry_add_data(entry, "MESSAGE=Dummy message");
  mock_entry_add_data(entry, "_PID=2345");
  mock_entry_add_data(entry, "_COMM=dummy");
  return entry;
}

void
_test_default_working_init(TestCase *self, TestSource *src, JournalReader *reader,
                           JournalReaderOptions *options)
{
  cfg->recv_time_zone = g_strdup("+06:00");
  self->user_data = options;

  MockEntry *entry = __create_dummy_entry("default_test");
  mock_journal_add_entry(entry);
}

void
_test_default_working_test(TestCase *self, TestSource *src, LogMessage *msg)
{
  const gchar *message = log_msg_get_value(msg, LM_V_MESSAGE, NULL);
  JournalReaderOptions *options = self->user_data;
  cr_assert_str_eq(message, "Dummy message", "%s", "Bad message");
  cr_assert_eq(msg->pri, options->default_pri, "%s", "Bad default prio");
  cr_assert_eq(options->fetch_limit, 10, "%s", "Bad default fetch_limit");
  cr_assert_eq(options->max_field_size, 64 * 1024, "%s", "Bad max field size");
  cr_assert_str_eq(options->prefix, ".journald.", "%s", "Bad default prefix value");
  cr_assert_str_eq(options->recv_time_zone, cfg->recv_time_zone, "%s", "Bad default timezone");
  test_source_finish_tc(src);
}

void
_test_prefix_init(TestCase *self, TestSource *src, JournalReader *reader,
                  JournalReaderOptions *options)
{
  options->prefix = g_strdup((gchar *)self->user_data);
  MockEntry *entry = __create_real_entry("prefix_test");
  mock_journal_add_entry(entry);
}

void
__test_other_has_prefix(TestCase *self, LogMessage *msg)
{
  gchar *requested_name = g_strdup_printf("%s%s", (gchar *) self->user_data, "_CMDLINE");
  gssize value_len;
  const gchar *value = log_msg_get_value_by_name(msg, requested_name, &value_len);
  cr_assert_str_eq(value, "sshd: foo_user [priv]", "%s", "Bad value for prefixed key");
  g_free(requested_name);
}

void
_test_prefix_test(TestCase *self, TestSource *src, LogMessage *msg)
{
  const gchar *message = log_msg_get_value(msg, LM_V_MESSAGE, NULL);
  cr_assert_str_eq(message, "pam_unix(sshd:session): session opened for user foo_user by (uid=0)",
                   "%s", "Bad message");

  __test_other_has_prefix(self, msg);

  test_source_finish_tc(src);
}

void
_test_field_size_init(TestCase *self, TestSource *src, JournalReader *reader,
                      JournalReaderOptions *options)
{
  options->max_field_size = GPOINTER_TO_INT(self->user_data);
  MockEntry *entry = __create_real_entry("field_size_test");
  mock_journal_add_entry(entry);
}

gboolean
__check_value_len(NVHandle handle, const gchar *name,
                  const gchar *value, gssize value_len,
                  LogMessageValueType type, gpointer user_data)
{
  TestCase *self = user_data;
  gchar *error_message = g_strdup_printf("Bad value size; name: %s, value: %s len: %ld", name, value, value_len);
  if (strcmp(name, "HOST_FROM") != 0 && strcmp(name, "TRANSPORT") != 0)
    {
      cr_assert_leq(value_len, GPOINTER_TO_INT(self->user_data), "%s", error_message);
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
_test_timezone_init(TestCase *self, TestSource *src, JournalReader *reader,
                    JournalReaderOptions *options)
{
  options->recv_time_zone = g_strdup("+09:00");

  MockEntry *entry = __create_real_entry("time_zone_test");
  mock_journal_add_entry(entry);
}

void
_test_timezone_test(TestCase *self, TestSource *src, LogMessage *msg)
{
  cr_assert_eq(msg->timestamps[LM_TS_STAMP].ut_gmtoff, 9 * 3600, "%s", "Bad time zone info");
  test_source_finish_tc(src);
}

void
_test_default_level_init(TestCase *self, TestSource *src, JournalReader *reader,
                         JournalReaderOptions *options)
{
  MockEntry *entry = __create_dummy_entry("test default level");
  gint level = GPOINTER_TO_INT(self->user_data);
  if (options->default_pri == 0xFFFF)
    options->default_pri = LOG_USER;
  options->default_pri = (options->default_pri & ~7) | level;

  mock_journal_add_entry(entry);
}

void
_test_default_level_test(TestCase *self, TestSource *src, LogMessage *msg)
{
  gint level = GPOINTER_TO_INT(self->user_data);
  cr_assert_eq(msg->pri, LOG_LOCAL0 | level, "%s", "Bad default prio");
  test_source_finish_tc(src);
}

void
_test_default_facility_init(TestCase *self, TestSource *src, JournalReader *reader,
                            JournalReaderOptions *options)
{
  MockEntry *entry = __create_dummy_entry("test default facility");
  gint facility = GPOINTER_TO_INT(self->user_data);
  if(options->default_pri == 0xFFFF)
    options->default_pri = LOG_NOTICE;
  options->default_pri = (options->default_pri & 7) | facility;
  mock_journal_add_entry(entry);
}

void
_test_default_facility_test(TestCase *self, TestSource *src, LogMessage *msg)
{
  gint facility = GPOINTER_TO_INT(self->user_data);
  cr_assert_eq(msg->pri, facility | LOG_NOTICE, "%s", "Bad default prio");
  test_source_finish_tc(src);
}

void
_test_program_field_init(TestCase *self, TestSource *src, JournalReader *reader,
                         JournalReaderOptions *options)
{
  MockEntry *entry = mock_entry_new("test _COMM first win");
  mock_entry_add_data(entry, "_COMM=comm_program");
  mock_entry_add_data(entry, "SYSLOG_IDENTIFIER=syslog_program");
  mock_journal_add_entry(entry);

  entry = mock_entry_new("test _COMM second win");
  mock_entry_add_data(entry, "SYSLOG_IDENTIFIER=syslog_program");
  mock_entry_add_data(entry, "_COMM=comm_program");
  mock_journal_add_entry(entry);

  entry = mock_entry_new("no SYSLOG_IDENTIFIER");
  mock_entry_add_data(entry, "_COMM=comm_program");
  mock_journal_add_entry(entry);

  self->user_data = reader;
}

void
_test_program_field_test(TestCase *self, TestSource *src, LogMessage *msg)
{
  JournalReader *reader = self->user_data;
  sd_journal *journal = journal_reader_get_sd_journal(reader);
  gchar *cursor;
  sd_journal_get_cursor(journal, &cursor);
  if (strcmp(cursor, "no SYSLOG_IDENTIFIER") != 0)
    {
      cr_assert_str_eq(log_msg_get_value(msg, LM_V_PROGRAM, NULL), "syslog_program", "%s", "Bad program name");
      g_free(cursor);
    }
  else
    {
      cr_assert_str_eq(log_msg_get_value(msg, LM_V_PROGRAM, NULL), "comm_program", "%s", "Bad program name");
      g_free(cursor);
      test_source_finish_tc(src);
    }
}

Test(systemd_journal, test_journal_reader)
{
  const gchar *persist_file = "test_systemd_journal55.persist";

  _init_cfg_with_persist_file(persist_file);

  TestSource *src = test_source_new(cfg);
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

  _deinit_cfg(persist_file);
}

#if SYSLOG_NG_HAVE_JOURNAL_NAMESPACES
Test(systemd_journal, test_journal_reader_namespace_init_same_twice)
{
  const gchar *persist_file = "test_systemd_journal_namespace_init.persist";
  _init_cfg_with_persist_file(persist_file);

  void *test_1 = journal_reader_test_prepare_with_namespace("asd", cfg);
  cr_assert(journal_reader_test_allocate_namespace(test_1), "%s", "Can't initialize first reader");

  void *test_2 = journal_reader_test_prepare_with_namespace("asd", cfg);
  cr_assert_not(journal_reader_test_allocate_namespace(test_2), "%s",
                "Multiple readers with the same namespace initialized");

  journal_reader_test_destroy(test_1);
  journal_reader_test_destroy(test_2);

  _deinit_cfg(persist_file);
}
#endif

TestSuite(systemd_journal, .init = app_startup, .fini = app_shutdown);
