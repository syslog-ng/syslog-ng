/*
 * Copyright (c) 2026 One Identity LLC.
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
#include <criterion/parameterized.h>

#include "afsql_test_helpers.h"
#include "apphook.h"

/* ----------------------------- fixtures ----------------------------- */

static AFSqlDestDriver *driver;

static void
setup(void)
{
  app_startup();
  mock_dbi_reset();
  driver = _create_driver();
}

static void
teardown(void)
{
  mock_dbi_reset();
  _free_driver(driver);
  app_shutdown();
}

TestSuite(afsql_dd_run_query, .init = setup, .fini = teardown);

/* ----------------------------- tests -------------------------------- */

Test(afsql_dd_run_query, successful_query_returns_true)
{
  mock_dbi_query_result = (dbi_result) 0x1;

  gboolean ret = afsql_dd_run_query(driver, "SELECT 1", FALSE, NULL);

  cr_assert(ret, "Expected TRUE on successful query");
}

Test(afsql_dd_run_query, failed_query_returns_false)
{
  mock_dbi_query_result = NULL;

  gboolean ret = afsql_dd_run_query(driver, "SELECT 1", FALSE, NULL);

  cr_assert_not(ret, "Expected FALSE when dbi_conn_query returns NULL");
}

Test(afsql_dd_run_query, result_pointer_is_populated_on_success)
{
  dbi_result sentinel = (dbi_result) 0xdeadbeef;
  mock_dbi_query_result = sentinel;

  dbi_result out = NULL;
  afsql_dd_run_query(driver, "SELECT 1", FALSE, &out);

  cr_assert_eq(out, sentinel,
               "Expected *result to hold the dbi_result returned by dbi_conn_query");
}

Test(afsql_dd_run_query, result_is_freed_when_no_pointer_given)
{
  mock_dbi_query_result = (dbi_result) 0x1;
  mock_dbi_result_freed = FALSE;

  afsql_dd_run_query(driver, "SELECT 1", FALSE, NULL);

  cr_assert(mock_dbi_result_freed,
            "Expected dbi_result_free() to be called when result pointer is NULL");
}

Test(afsql_dd_run_query, result_is_not_freed_when_pointer_given)
{
  mock_dbi_query_result = (dbi_result) 0x1;
  mock_dbi_result_freed = FALSE;

  dbi_result out = NULL;
  afsql_dd_run_query(driver, "SELECT 1", FALSE, &out);

  cr_assert_not(mock_dbi_result_freed,
                "Expected dbi_result_free() NOT to be called when result pointer is provided");
}

Test(afsql_dd_run_query, silent_mode_suppresses_error_on_failure)
{
  mock_dbi_query_result = NULL;

  /* Should not crash or assert even though the query fails */
  gboolean ret = afsql_dd_run_query(driver, "BAD QUERY", TRUE, NULL);

  cr_assert_not(ret, "Expected FALSE on failure regardless of silent flag");
}

/* SQL is already quoted by the selected libdbi backend and must be forwarded unchanged. */
typedef struct
{
  gchar description[64];
  gchar raw_query[128];
} QuerySanitizationParam;

ParameterizedTestParameters(afsql_dd_run_query, query_sanitization)
{
  static QuerySanitizationParam params[] =
  {
    { "plain ascii select", "SELECT 1" },
    { "query with newline", "INSERT INTO t (v) VALUES ('line1\nline2')" },
    { "query with backslash", "SELECT '\\'" },
    { "invalid utf-8 byte", "SELECT '\xad'" },
    { "valid utf-8 multibyte", "SELECT 'árvíztűrőtükörfúrógép'" },
    { "empty query string", "" },
  };

  return cr_make_param_array(QuerySanitizationParam, params,
                             sizeof(params) / sizeof(params[0]));
}

ParameterizedTest(QuerySanitizationParam *p, afsql_dd_run_query, query_sanitization)
{
  mock_dbi_reset();
  mock_dbi_query_result = (dbi_result) 0x1;

  afsql_dd_run_query(driver, p->raw_query, FALSE, NULL);

  cr_assert_not_null(mock_dbi_last_query,
                     "[%s] Expected dbi_conn_query to have been called", p->description);

  cr_assert_str_eq(mock_dbi_last_query, p->raw_query,
                   "[%s] Query was modified before reaching libdbi.\n  got:      %s\n  expected: %s",
                   p->description, mock_dbi_last_query, p->raw_query);
}

Test(afsql_dd_run_query, preserves_mysql_quoted_value)
{
  const gchar *columns[] = { "msg" };
  const gchar *templates[] = { "${MSG}" };
  _set_fields(driver, columns, templates, 1);

  LogMessage *msg = log_msg_new_empty();
  log_msg_set_value_by_name(msg, "MSG", "x\\y and z'w", -1);
  GString *table = _make_table("logs");
  GString *query = afsql_dd_build_insert_command(driver, msg, table);

  afsql_dd_run_query(driver, query->str, FALSE, NULL);

  cr_assert_str_eq(mock_dbi_last_query,
                   "INSERT INTO logs (msg) VALUES ('x\\y and z\\'w')");

  g_string_free(query, TRUE);
  g_string_free(table, TRUE);
  log_msg_unref(msg);
}
