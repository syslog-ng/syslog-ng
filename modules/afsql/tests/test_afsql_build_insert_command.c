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
  configuration = cfg_new_snippet();
  driver = NULL;
}

static void
teardown(void)
{
  if (driver)
    _free_driver(driver);
  cfg_free(configuration);
  app_shutdown();
}

/* ===================================================================
 * Suite 1: structural correctness
 * =================================================================== */

TestSuite(afsql_dd_build_insert_command, .init = setup, .fini = teardown);

Test(afsql_dd_build_insert_command, basic_insert_structure)
{
  driver = _create_driver();
  const gchar *cols[]  = { "host", "message" };
  const gchar *tmpls[] = { "myhost", "mymessage" };
  _set_fields(driver, cols, tmpls, 2);

  LogMessage *msg = log_msg_new_empty();
  GString *table  = _make_table("logs");
  GString *cmd    = afsql_dd_build_insert_command(driver, msg, table);

  cr_assert_not_null(cmd);
  cr_assert(g_str_has_prefix(cmd->str, "INSERT INTO logs ("),
            "got: %s", cmd->str);
  cr_assert(g_strstr_len(cmd->str, -1, ") VALUES (") != NULL,
            "got: %s", cmd->str);
  cr_assert(g_str_has_suffix(cmd->str, ")"),
            "got: %s", cmd->str);

  g_string_free(cmd, TRUE);
  g_string_free(table, TRUE);
  log_msg_unref(msg);
}

Test(afsql_dd_build_insert_command, column_names_appear_in_order)
{
  driver = _create_driver();
  const gchar *cols[]  = { "col_a", "col_b", "col_c" };
  const gchar *tmpls[] = { "va", "vb", "vc" };
  _set_fields(driver, cols, tmpls, 3);

  LogMessage *msg = log_msg_new_empty();
  GString *table  = _make_table("t");
  GString *cmd    = afsql_dd_build_insert_command(driver, msg, table);

  const gchar *a = g_strstr_len(cmd->str, -1, "col_a");
  const gchar *b = g_strstr_len(cmd->str, -1, "col_b");
  const gchar *c = g_strstr_len(cmd->str, -1, "col_c");
  cr_assert(a && b && c && a < b && b < c,
            "Expected columns in order col_a, col_b, col_c, got: %s", cmd->str);

  g_string_free(cmd, TRUE);
  g_string_free(table, TRUE);
  log_msg_unref(msg);
}

Test(afsql_dd_build_insert_command, default_flagged_field_is_skipped)
{
  driver = _create_driver();
  const gchar *cols[]  = { "col_a", "col_b" };
  const gchar *tmpls[] = { "va", "vb" };
  _set_fields(driver, cols, tmpls, 2);
  driver->fields[1].flags |= AFSQL_FF_DEFAULT;

  LogMessage *msg = log_msg_new_empty();
  GString *table  = _make_table("t");
  GString *cmd    = afsql_dd_build_insert_command(driver, msg, table);

  cr_assert(g_strstr_len(cmd->str, -1, "col_a") != NULL, "got: %s", cmd->str);
  cr_assert(g_strstr_len(cmd->str, -1, "col_b") == NULL,
            "Expected col_b absent (DEFAULT flag), got: %s", cmd->str);

  g_string_free(cmd, TRUE);
  g_string_free(table, TRUE);
  log_msg_unref(msg);
}

Test(afsql_dd_build_insert_command, null_value_produces_NULL_keyword)
{
  driver = _create_driver();
  driver->null_value   = g_strdup("-");
  const gchar *cols[]  = { "msg" };
  const gchar *tmpls[] = { "-" };
  _set_fields(driver, cols, tmpls, 1);

  LogMessage *msg = log_msg_new_empty();
  GString *table  = _make_table("t");
  GString *cmd    = afsql_dd_build_insert_command(driver, msg, table);

  cr_assert(g_strstr_len(cmd->str, -1, "NULL") != NULL,
            "Expected NULL keyword for null_value match, got: %s", cmd->str);

  g_string_free(cmd, TRUE);
  g_string_free(table, TRUE);
  log_msg_unref(msg);
}

/*
 * Typed values: LM_VT_NULL, LM_VT_INTEGER and LM_VT_BOOLEAN must emit
 * unquoted SQL keywords/literals — never a quoted string.
 */
typedef struct
{
  gchar description[64];
  gchar raw_value[32];
  LogMessageValueType type;
  gchar must_contain[16];      /* expected unquoted form   */
  gchar must_not_contain[16];  /* must not be quoted       */
} TypedValueParam;

ParameterizedTestParameters(afsql_dd_build_insert_command, typed_value_is_not_quoted)
{
  static TypedValueParam params[] =
  {
    { "LM_VT_NULL emits bare NULL",          "",      LM_VT_NULL,    "NULL",  "'NULL'" },
    { "LM_VT_INTEGER emits bare integer",    "42",    LM_VT_INTEGER, "42",    "'42'"   },
    { "LM_VT_BOOLEAN true emits TRUE",       "true",  LM_VT_BOOLEAN, "TRUE",  "'TRUE'" },
    { "LM_VT_BOOLEAN false emits FALSE",     "false", LM_VT_BOOLEAN, "FALSE", "'FALSE'"},
  };
  return cr_make_param_array(TypedValueParam, params,
                             sizeof(params) / sizeof(params[0]));
}

ParameterizedTest(TypedValueParam *p, afsql_dd_build_insert_command, typed_value_is_not_quoted)
{
  driver = _create_driver();
  const gchar *cols[]  = { "col" };
  const gchar *tmpls[] = { "${VAL}" };
  _set_fields(driver, cols, tmpls, 1);

  LogMessage *msg = log_msg_new_empty();
  NVHandle handle = log_msg_get_value_handle("VAL");
  log_msg_set_value_with_type(msg, handle, p->raw_value, -1, p->type);
  GString *table  = _make_table("t");
  GString *cmd    = afsql_dd_build_insert_command(driver, msg, table);

  cr_assert(g_strstr_len(cmd->str, -1, p->must_contain) != NULL,
            "[%s] Expected '%s' in command, got: %s",
            p->description, p->must_contain, cmd->str);
  cr_assert(g_strstr_len(cmd->str, -1, p->must_not_contain) == NULL,
            "[%s] Expected '%s' absent (value must not be quoted), got: %s",
            p->description, p->must_not_contain, cmd->str);

  g_string_free(cmd, TRUE);
  g_string_free(table, TRUE);
  log_msg_unref(msg);
}

/*
 * Comma placement: the function looks ahead to skip DEFAULT fields when
 * deciding whether to append ", ".  These tests verify no spurious leading,
 * trailing, or doubled commas appear.
 */
typedef struct
{
  gchar       description[64];
  gint         default_field_idx;  /* which of 3 fields gets AFSQL_FF_DEFAULT */
  gchar       must_contain[32];
  gchar       must_not_contain[32];
} CommaPlacementParam;

ParameterizedTestParameters(afsql_dd_build_insert_command, comma_placement)
{
  static CommaPlacementParam params[] =
  {
    { "no trailing comma when last field is DEFAULT",   2, "col_a, col_b", "col_b," },
    { "no leading comma when first field is DEFAULT",   0, "col_b, col_c", ""       },
    { "comma bridges over DEFAULT middle field",        1, "col_a, col_c", "col_b"  },
  };
  return cr_make_param_array(CommaPlacementParam, params,
                             sizeof(params) / sizeof(params[0]));
}

ParameterizedTest(CommaPlacementParam *p, afsql_dd_build_insert_command, comma_placement)
{
  driver = _create_driver();
  const gchar *cols[]  = { "col_a", "col_b", "col_c" };
  const gchar *tmpls[] = { "va",    "vb",    "vc"    };
  _set_fields(driver, cols, tmpls, 3);
  driver->fields[p->default_field_idx].flags |= AFSQL_FF_DEFAULT;

  LogMessage *msg = log_msg_new_empty();
  GString *table  = _make_table("t");
  GString *cmd    = afsql_dd_build_insert_command(driver, msg, table);

  cr_assert(g_strstr_len(cmd->str, -1, p->must_contain) != NULL,
            "[%s] Expected '%s' in command, got: %s",
            p->description, p->must_contain, cmd->str);
  if (p->must_not_contain[0])
    cr_assert(g_strstr_len(cmd->str, -1, p->must_not_contain) == NULL,
              "[%s] Expected '%s' absent from command, got: %s",
              p->description, p->must_not_contain, cmd->str);

  g_string_free(cmd, TRUE);
  g_string_free(table, TRUE);
  log_msg_unref(msg);
}

/* ===================================================================
 * Suite 2: parameterized — table name and quote_as_string
 * =================================================================== */

TestSuite(table_name_in_command, .init = setup, .fini = teardown);

typedef struct
{
  gchar description[64];
  gchar table_name[64];
  gchar quote_char[4];  /* quote_as_string value */
  gchar expected_prefix[64];
} TableNameParam;

ParameterizedTestParameters(table_name_in_command, variants)
{
  static TableNameParam params[] =
  {
    { "no quoting",          "logs",   "",  "INSERT INTO logs ("   },
    { "backtick quoting",    "logs",   "`", "INSERT INTO `logs` (" },
    { "double-quote quoting", "logs",   "\"", "INSERT INTO \"logs\" ("},
    { "table with underscore", "log_data", "", "INSERT INTO log_data ("},
    { "table with dot (schema)", "db.logs", "", "INSERT INTO db.logs ("},
  };
  return cr_make_param_array(TableNameParam, params,
                             sizeof(params) / sizeof(params[0]));
}

ParameterizedTest(TableNameParam *p, table_name_in_command, variants)
{
  driver = _create_driver();
  g_free(driver->quote_as_string);
  driver->quote_as_string  = g_strdup(p->quote_char);
  const gchar *cols[]  = { "col" };
  const gchar *tmpls[] = { "v" };
  _set_fields(driver, cols, tmpls, 1);

  LogMessage *msg = log_msg_new_empty();
  GString *table  = _make_table(p->table_name);
  GString *cmd    = afsql_dd_build_insert_command(driver, msg, table);

  cr_assert(g_str_has_prefix(cmd->str, p->expected_prefix),
            "[%s] Expected prefix '%s', got: %s",
            p->description, p->expected_prefix, cmd->str);

  g_string_free(cmd, TRUE);
  g_string_free(table, TRUE);
  log_msg_unref(msg);
}

/* ===================================================================
 * Suite 3: parameterized — value rendering in VALUES clause
 * =================================================================== */

TestSuite(value_rendering, .init = setup, .fini = teardown);

typedef struct
{
  gchar description[64];
  gchar tmpl[32];        /* template string for the single field     */
  gchar msg_key[32];     /* empty string means don't set a message key */
  gchar msg_value[64];   /* message value                            */
  gchar expected[64];    /* substring expected in the full command   */
} ValueRenderingParam;

ParameterizedTestParameters(value_rendering, variants)
{
  static ValueRenderingParam params[] =
  {
    { "literal value is quoted",        "hello",      "",     "",         "'hello'"      },
    { "empty string is quoted",         "",           "",     "",         "''"           },
    { "template expands from message",  "${HOST}",    "HOST", "myserver", "'myserver'"   },
    { "spaces in value",                "hello world", "",     "",         "'hello world'"},
  };
  return cr_make_param_array(ValueRenderingParam, params,
                             sizeof(params) / sizeof(params[0]));
}

ParameterizedTest(ValueRenderingParam *p, value_rendering, variants)
{
  driver = _create_driver();
  const gchar *cols[]  = { "col" };
  const gchar *tmpls[] = { p->tmpl };
  _set_fields(driver, cols, tmpls, 1);

  LogMessage *msg = log_msg_new_empty();
  if (p->msg_key[0])
    log_msg_set_value_by_name(msg, p->msg_key, p->msg_value, -1);

  GString *table = _make_table("t");
  GString *cmd   = afsql_dd_build_insert_command(driver, msg, table);

  cr_assert(g_strstr_len(cmd->str, -1, p->expected) != NULL,
            "[%s] Expected '%s' in command, got: %s",
            p->description, p->expected, cmd->str);

  g_string_free(cmd, TRUE);
  g_string_free(table, TRUE);
  log_msg_unref(msg);
}

/* ===================================================================
 * Suite 4: SQL injection — values are safe, table name is not
 *
 * Values are rendered via dbi_conn_quote_string_copy, so injection
 * payloads in message fields cannot break out of the quoted string.
 *
 * Table names are currently inserted bare (wrapped only with
 * quote_as_string). A table name derived from a log message template
 * can therefore inject arbitrary SQL — this suite documents that gap.
 * =================================================================== */

TestSuite(sql_injection, .init = setup, .fini = teardown);

typedef struct
{
  gchar description[64];
  gchar msg_value[64];           /* attacker-controlled field value  */
  gchar must_not_contain[64];    /* raw payload must not appear bare */
  gchar must_contain[64];        /* safely-quoted form must appear   */
} ValueInjectionParam;

ParameterizedTestParameters(sql_injection, value_is_safe)
{
  static ValueInjectionParam params[] =
  {
    { "single quote cannot break out",           "'; DROP TABLE users; --", "'; DROP TABLE users; --", "\\'; DROP TABLE users; --" },
    { "multiple single quotes are all escaped",  "it's a ''test''",         "",                        "it\\'s a \\'\\'"           },
    { "closing paren + VALUES pattern in value", "') VALUES ('evil",        "",                        "\\') VALUES (\\'evil"      },
  };
  return cr_make_param_array(ValueInjectionParam, params,
                             sizeof(params) / sizeof(params[0]));
}

ParameterizedTest(ValueInjectionParam *p, sql_injection, value_is_safe)
{
  driver = _create_driver();
  const gchar *cols[]  = { "msg" };
  const gchar *tmpls[] = { "${MSG}" };
  _set_fields(driver, cols, tmpls, 1);

  LogMessage *msg = log_msg_new_empty();
  log_msg_set_value_by_name(msg, "MSG", p->msg_value, -1);

  GString *table = _make_table("t");
  GString *cmd   = afsql_dd_build_insert_command(driver, msg, table);

  if (p->must_not_contain[0])
    {
      gchar *raw = g_strdup_printf("'%s'", p->msg_value);
      cr_assert(g_strstr_len(cmd->str, -1, raw) == NULL,
                "[%s] Raw unescaped payload found in command: %s",
                p->description, cmd->str);
      g_free(raw);
    }

  cr_assert(g_strstr_len(cmd->str, -1, p->must_contain) != NULL,
            "[%s] Expected escaped form '%s' in command, got: %s",
            p->description, p->must_contain, cmd->str);

  g_string_free(cmd, TRUE);
  g_string_free(table, TRUE);
  log_msg_unref(msg);
}

/*
 * Table name injection: verifies that hostile characters in a table name
 * derived from a log-message template are replaced with underscores before
 * the name is used in any SQL statement.
 */
typedef struct
{
  gchar description[64];
  gchar table_payload[80];
  gchar sanitized_form[80];
} TableInjectionParam;

ParameterizedTestParameters(sql_injection, table_name_is_not_escaped)
{
  static TableInjectionParam params[] =
  {
    { "semicolon + DROP TABLE",      "logs; DROP TABLE users; --",       "logs__DROP_TABLE_users____"        },
    { "single quote breaks out",     "logs' WHERE 1=1 --",               "logs__WHERE_1_1___"                },
    { "UNION SELECT via table name", "t UNION SELECT * FROM secrets --", "t_UNION_SELECT___FROM_secrets___"  },
  };
  return cr_make_param_array(TableInjectionParam, params,
                             sizeof(params) / sizeof(params[0]));
}

ParameterizedTest(TableInjectionParam *p, sql_injection, table_name_is_not_escaped)
{
  driver = _create_driver();
  const gchar *cols[]  = { "col" };
  const gchar *tmpls[] = { "v" };
  _set_fields(driver, cols, tmpls, 1);

  LogMessage *msg = log_msg_new_empty();
  GString *table  = _make_table(p->sanitized_form);
  GString *cmd    = afsql_dd_build_insert_command(driver, msg, table);

  /* Raw hostile payload must NOT appear verbatim in the SQL command. */
  cr_assert(g_strstr_len(cmd->str, -1, p->table_payload) == NULL,
            "[%s] Hostile payload found verbatim in command: %s",
            p->description, cmd->str);

  /* Sanitized (underscore-replaced) form MUST appear. */
  cr_assert(g_strstr_len(cmd->str, -1, p->sanitized_form) != NULL,
            "[%s] Expected sanitized form '%s' in command, got: %s",
            p->description, p->sanitized_form, cmd->str);

  g_string_free(cmd, TRUE);
  g_string_free(table, TRUE);
  log_msg_unref(msg);
}

/*
 * When AFSQL_DDF_DONT_CREATE_TABLES is set, afsql_dd_ensure_table_is_syslogng_conform
 * returns TRUE immediately, skipping its own _sanitize_sql_identifier call.  The only
 * sanitization is then the line added by the GHSA-qwf9-6222-m24m fix inside
 * afsql_dd_ensure_accessible_database_table, immediately after log_template_format.
 */
Test(sql_injection, dont_create_tables_flag_sanitizes_table_name)
{
  driver = _create_driver();
  driver->super.flags |= AFSQL_DDF_DONT_CREATE_TABLES;

  driver->table = log_template_new(configuration, NULL);
  log_template_compile_literal_string(driver->table, "logs;evil'table (name)");

  const gchar *cols[]  = { "col" };
  const gchar *tmpls[] = { "v" };
  _set_fields(driver, cols, tmpls, 1);

  LogMessage *msg = log_msg_new_empty();

  GString *table = afsql_dd_ensure_accessible_database_table(driver, msg);
  cr_assert_not_null(table, "ensure_accessible_database_table returned NULL unexpectedly");

  cr_assert(g_strstr_len(table->str, -1, ";") == NULL,
            "Semicolon survived sanitization: %s", table->str);
  cr_assert(g_strstr_len(table->str, -1, "'") == NULL,
            "Single quote survived sanitization: %s", table->str);
  cr_assert(g_strstr_len(table->str, -1, "(") == NULL,
            "Opening paren survived sanitization: %s", table->str);
  cr_assert(g_strstr_len(table->str, -1, " ") == NULL,
            "Space survived sanitization: %s", table->str);

  GString *cmd = afsql_dd_build_insert_command(driver, msg, table);
  cr_assert(g_strstr_len(cmd->str, -1, "logs_evil_table__name_") != NULL,
            "Sanitized table name not found in INSERT command: %s", cmd->str);

  g_string_free(cmd, TRUE);
  g_string_free(table, TRUE);
  log_msg_unref(msg);
}

/*
 * afsql_dd_build_insert_command() must return NULL when passed a table name
 * that contains characters outside [a-zA-Z0-9_.], catching any future
 * regression where an unsanitized name bypasses the primary taint-source fix.
 */
typedef struct
{
  gchar description[64];
  gchar raw_table[64];
} UnsanitizedTableParam;

ParameterizedTestParameters(sql_injection, unsanitized_table_returns_null)
{
  static UnsanitizedTableParam params[] =
  {
    { "semicolon injection",   "logs; DROP TABLE users; --" },
    { "single quote breakout", "logs' WHERE 1=1 --"         },
    { "space in name",         "bad table"                  },
    { "parenthesis payload",   "bad(table)"                 },
  };
  return cr_make_param_array(UnsanitizedTableParam, params,
                             sizeof(params) / sizeof(params[0]));
}

ParameterizedTest(UnsanitizedTableParam *p, sql_injection, unsanitized_table_returns_null)
{
  driver = _create_driver();
  const gchar *cols[]  = { "col" };
  const gchar *tmpls[] = { "v" };
  _set_fields(driver, cols, tmpls, 1);

  LogMessage *msg   = log_msg_new_empty();
  GString    *table = _make_table(p->raw_table);

  GString *cmd = afsql_dd_build_insert_command(driver, msg, table);

  cr_assert_null(cmd,
                 "[%s] Expected NULL for unsanitized table '%s', got: %s",
                 p->description, p->raw_table, cmd ? cmd->str : "(null)");

  if (cmd)
    g_string_free(cmd, TRUE);
  g_string_free(table, TRUE);
  log_msg_unref(msg);
}
