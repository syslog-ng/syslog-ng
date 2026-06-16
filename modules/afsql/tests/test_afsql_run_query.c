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

/*
 * Parameterized regression tests for the SQL injection fix (commit e9dbd15bf).
 *
 * Each entry describes a raw query string and the exact string that must
 * arrive at dbi_conn_query after convert_unsafe_utf8_to_escaped_text has
 * processed it.  A NULL expected_query means the input is safe ASCII and
 * must pass through byte-for-byte unchanged.
 */
typedef struct
{
  gchar description[64];
  gchar raw_query[128];
  gchar expected_query[128];
  gboolean expected_query_is_null; /* TRUE → identical to raw_query */
} QuerySanitizationParam;

ParameterizedTestParameters(afsql_dd_run_query, query_sanitization)
{
  static QuerySanitizationParam params[] =
  {
    /* safe inputs — must be forwarded unchanged */
    { "plain ascii select",                       "SELECT 1",                                       "", TRUE },
    { "insert with safe string value",            "INSERT INTO logs (msg) VALUES ('hello world')",  "", TRUE },
    { "query with numbers and underscores",       "SELECT id, log_level FROM events WHERE id = 42", "", TRUE },

    /* control characters that must be escaped */
    { "bell character \\x07",                     "SELECT '\x07'",                                  "SELECT '\\x07'",                              FALSE },
    { "newline",                                  "INSERT INTO t (v) VALUES ('line1\nline2')",      "INSERT INTO t (v) VALUES ('line1\\nline2')",  FALSE },
    { "carriage return",                          "INSERT INTO t (v) VALUES ('a\rb')",              "INSERT INTO t (v) VALUES ('a\\rb')",          FALSE },
    { "tab",                                      "INSERT INTO t (v) VALUES ('col1\tcol2')",        "INSERT INTO t (v) VALUES ('col1\\tcol2')",    FALSE },
    { "backspace",                                "INSERT INTO t (v) VALUES ('\b')",                "INSERT INTO t (v) VALUES ('\\b')",            FALSE },
    { "form feed",                                "INSERT INTO t (v) VALUES ('\f')",                "INSERT INTO t (v) VALUES ('\\f')",            FALSE },
    { "soh \\x01 unnamed control char",           "SELECT '\x01'",                                  "SELECT '\\x01'",                              FALSE },
    { "unit separator \\x1f",                     "SELECT '\x1f'",                                  "SELECT '\\x1f'",                              FALSE },
    { "multiple consecutive control chars",       "\x01\x02\x03",                                   "\\x01\\x02\\x03",                             FALSE },
    { "backslash is doubled",                     "SELECT '\\'",                                    "SELECT '\\\\'",                               FALSE },

    /* a literal backslash-n in the input (two chars: \ + n) must become \\n,
     * not be re-interpreted as a newline escape */
    { "pre-escaped \\n is not re-interpreted",    "SELECT '\\n'",                                   "SELECT '\\\\n'",                              FALSE },

    /* invalid / overlong UTF-8 sequences */
    { "invalid utf-8 byte \\xad is hex-escaped",  "SELECT '\xad'",                                  "SELECT '\\\\xad'",                            FALSE },
    { "invalid utf-8 byte surrounded by valid",   "SELECT 'Á\xadÉ'",                                "SELECT 'Á\\\\xadÉ'",                          FALSE },
    { "truncated 2-byte utf-8 start byte",        "SELECT '\xc3'",                                  "SELECT '\\\\xc3'",                            FALSE },
    { "multiple consecutive invalid utf-8 bytes", "SELECT '\xad\xae'",                              "SELECT '\\\\xad\\\\xae'",                     FALSE },

    /* SQL metacharacters: quotes and semicolons are NOT escaped
     * (unsafe_flags=0 — the escaping targets binary/control safety, not SQL) */
    { "single quote passes through",              "SELECT ''''",                                    "", TRUE },
    { "double quote passes through",              "SELECT \"val\"",                                 "", TRUE },
    { "semicolon passes through",                 "SELECT 1; DROP TABLE users",                     "", TRUE },

    /* DEL (0x7f) is >= 32 and not a backslash, so it passes through */
    { "del character 0x7f passes through",        "SELECT '\x7f'",                                  "", TRUE },

    /* valid multibyte UTF-8 must not be mangled */
    { "valid utf-8 multibyte passes through",     "SELECT 'árvíztűrőtükörfúrógép'",                 "", TRUE },
    { "valid utf-8 followed by newline",          "SELECT 'árvíztűrőtükörfúrógép\n'",               "SELECT 'árvíztűrőtükörfúrógép\\n'",           FALSE },

    /* empty query edge case */
    { "empty query string",                       "",                                               "", TRUE },
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

  const gchar *expected = p->expected_query_is_null ? p->raw_query : p->expected_query;
  cr_assert_str_eq(mock_dbi_last_query, expected,
                   "[%s] Sanitised query mismatch.\n  got:      %s\n  expected: %s",
                   p->description, mock_dbi_last_query, expected);
}
