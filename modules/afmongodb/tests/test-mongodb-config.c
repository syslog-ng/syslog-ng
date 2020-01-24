/*
 * Copyright (c) 2016 Balabit
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "syslog-ng.h"
#include "testutils.h"
#include "mainloop.h"
#include "modules/afmongodb/afmongodb-parser.h"
#include "logthrdest/logthrdestdrv.h"

static int _tests_failed = 0;
static GlobalConfig *test_cfg;
static LogDriver *mongodb;

#define SAFEOPTS "?wtimeoutMS=60000&socketTimeoutMS=60000&connectTimeoutMS=60000"

static void
_before_test(void)
{
  start_grabbing_messages();
  mongodb = afmongodb_dd_new(test_cfg);
}

static gboolean
_after_test(void)
{
  gboolean uri_init_ok = afmongodb_dd_private_uri_init(mongodb);
  stop_grabbing_messages();
  return uri_init_ok;
}

static void
_free_test(void)
{
  reset_grabbed_messages();
  if (mongodb)
    {
      log_pipe_unref(&mongodb->super);
      mongodb = NULL;
    }
}

typedef gboolean (*Checks)(const gchar *user_data);

static gboolean
_execute(const gchar *testcase, Checks checks, const gchar *user_data)
{
  gboolean uri_init_ok = _after_test();

  testcase_begin("%s(%s, %s)", __FUNCTION__, testcase, user_data);
  if (!checks(user_data))
    _tests_failed = 1;
  reset_grabbed_messages();
  testcase_end();

  _free_test();
  _before_test();
  return uri_init_ok;
}

static gboolean
_execute_find_text_in_log(const gchar *pattern)
{
  return assert_grabbed_messages_contain_non_fatal(pattern, "mismatch");
}

static void
_log_error(const gchar *message)
{
  fprintf(stderr, "error: %s\n", message);
}

static void
_execute_correct(const gchar *testcase, Checks checks, const gchar *user_data)
{
  if (!_execute(testcase, checks, user_data))
    {
      _log_error("expected the subject to succeed, but it failed");
      _tests_failed = 1;
    }
}

static void
_execute_failing(const gchar *testcase, Checks checks, const gchar *user_data)
{
  if (_execute(testcase, checks, user_data))
    {
      _log_error("expected the subject to fail, but it succeeded");
      _tests_failed = 1;
    }
}

static void
_expect_error_in_log(const gchar *testcase, const gchar *pattern)
{
  _execute_failing(testcase, _execute_find_text_in_log, pattern);
}

static void
_expect_uri_in_log(const gchar *testcase, const gchar *uri, const gchar *db, const gchar *coll)
{
  GString *pattern = g_string_sized_new(0);
  g_string_append_printf(pattern,
                         "Initializing MongoDB destination;"
                         " uri='mongodb://%s', db='%s', collection='%s'",
                         uri, db, coll);
  _execute_correct(testcase, _execute_find_text_in_log, pattern->str);
  g_string_free(pattern, TRUE);
}

static gboolean
_execute_compare_persist_name(const gchar *expected_name)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *)mongodb;
  const gchar *name = log_pipe_get_persist_name((const LogPipe *)self);
  return assert_nstring_non_fatal(name, -1, expected_name, -1, "mismatch");
}

static void
_test_persist_name(void)
{
  afmongodb_dd_set_uri(mongodb, "mongodb://127.0.0.2:27018,localhost:1234/syslog"
                       SAFEOPTS "&replicaSet=x");
  _execute_correct("persist", _execute_compare_persist_name,
                   "afmongodb(127.0.0.2:27018,syslog,x,messages)");
}

static gboolean
_execute_compare_stats_name(const gchar *expected_name)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *)mongodb;
  const gchar *name = self->format_stats_instance(self);
  return assert_nstring_non_fatal(name, -1, expected_name, -1, "mismatch");
}

static void
_test_stats_name(void)
{
  afmongodb_dd_set_uri(mongodb, "mongodb://127.0.0.2:27018,localhost:1234/syslog"
                       SAFEOPTS "&replicaSet=x");
  _execute_correct("stats", _execute_compare_stats_name,
                   "mongodb,127.0.0.2:27018,syslog,x,messages");
}

static void
_test_uri_correct(void)
{
  _expect_uri_in_log("default_uri", "127.0.0.1:27017/syslog" SAFEOPTS, "syslog", "messages");

  afmongodb_dd_set_uri(mongodb, "mongodb://%2Ftmp%2Fmongo.sock/syslog");
  _expect_uri_in_log("socket", "%2Ftmp%2Fmongo.sock/syslog", "syslog", "messages");

  afmongodb_dd_set_uri(mongodb, "mongodb://localhost:1234/syslog-ng");
  _expect_uri_in_log("uri", "localhost:1234/syslog-ng", "syslog-ng", "messages");

  afmongodb_dd_set_collection(mongodb, "messages2");
  _expect_uri_in_log("collection", "127.0.0.1:27017/syslog" SAFEOPTS, "syslog", "messages2");
}

static void
_test_uri_error(void)
{
  afmongodb_dd_set_uri(mongodb, "INVALID-URI");
  _expect_error_in_log("invalid_uri", "Error parsing MongoDB URI; uri='INVALID-URI'");

  afmongodb_dd_set_uri(mongodb, "mongodb://127.0.0.1:27017/");
  _expect_error_in_log("missing_db", "Missing DB name from MongoDB URI;"
                       " uri='mongodb://127.0.0.1:27017/'");
}

static void
_setup(void)
{
  main_loop_thread_resource_init();
  stats_cluster_init();
  msg_init(FALSE);
  g_thread_init(NULL);

  debug_flag = TRUE;
  verbose_flag = TRUE;
  trace_flag = TRUE;

  log_msg_registry_init();

  test_cfg = cfg_new_snippet();
  g_assert(test_cfg);

  const gchar *persist_filename = "";
  test_cfg->state = persist_state_new(persist_filename);

  _before_test();
}

static void
_teardown(void)
{
  _free_test();
  if (test_cfg->persist)
    {
      persist_config_free(test_cfg->persist);
      test_cfg->persist = NULL;
    }
  cfg_free(test_cfg);

  log_msg_registry_deinit();
  msg_deinit();
  stats_cluster_deinit();
  main_loop_thread_resource_deinit();
}

int
main(int argc, char **argv)
{
  _setup();

  _test_persist_name();
  _test_stats_name();
  _test_uri_correct();
  _test_uri_error();

  _teardown();
  return _tests_failed;
}
