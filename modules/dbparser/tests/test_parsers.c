/*
 * Copyright (c) 2013 Balabit
 * Copyright (c) 2013 Balázs Scheidler <balazs.scheidler@balabit.com>
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

#include "testutils.h"
#include <string.h>

/* NOTE: including the implementation file to access static functions */
#include "radix.c"


#define PARSER_TESTCASE(x, ...) do { parser_testcase_begin(#x, #__VA_ARGS__); x(__VA_ARGS__); parser_testcase_end(); } while(0)

#define parser_testcase_begin(func, args)                    	\
  do                                                            \
    {                                                           \
      testcase_begin("%s(%s)", func, args);                     \
    }                                                           \
  while (0)

#define parser_testcase_end()                                	\
  do                                                            \
    {                                                           \
      testcase_end();                                           \
    }                                                           \
  while (0)



static gboolean
_invoke_parser(gboolean (*parser)(guint8 *str, gint *len, const gchar *param, gpointer state, RParserMatch *match), const gchar *str, gpointer param, gpointer state, gchar **result_string)
{
  gboolean result;
  gint len;
  guint8 *dup = g_strdup(str);
  RParserMatch match;

  memset(&match, 0, sizeof(match));
  result = parser(dup, &len, param, state, &match);

  if (match.match)
    {
      if (result)
        *result_string = g_strdup(match.match);
    }
  else
    {
      match.ofs = 0 + match.ofs;
      match.len = (gint16) match.len + len;
      if (result)
        *result_string = g_strndup(&dup[match.ofs], match.len);
    }
  g_free(dup);
  return result;
}

static void
assert_parser_success(gboolean (*parser)(guint8 *str, gint *len, const gchar *param, gpointer state, RParserMatch *match), const gchar *str, gpointer param, gpointer state, const gchar *expected_string)
{
  gchar *result_string;
  gboolean result;

  result = _invoke_parser(parser, str, param, state, &result_string);
  assert_true(result, "Mismatching parser result");
  assert_string(result_string, expected_string, "Mismatching parser result");
  g_free(result_string);
}

static void
assert_parser_failure(gboolean (*parser)(guint8 *str, gint *len, const gchar *param, gpointer state, RParserMatch *match), const gchar *str, gpointer param, gpointer state)
{
  gboolean result;

  result = _invoke_parser(parser, str, param, state, NULL);
  assert_false(result, "Mismatching parser result");
}

static void
test_string_parser_without_parameter_parses_a_word(void)
{
  assert_parser_success(r_parser_string, "foo", NULL, NULL, "foo");
  assert_parser_success(r_parser_string, "foo bar", NULL, NULL, "foo");
  assert_parser_success(r_parser_string, "foo123 bar", NULL, NULL, "foo123");
  assert_parser_success(r_parser_string, "foo{}", NULL, NULL, "foo");
  assert_parser_success(r_parser_string, "foo[]", NULL, NULL, "foo");
  assert_parser_failure(r_parser_string, "", NULL, NULL);
}

static void
test_string_parser_with_additional_end_characters(void)
{
  assert_parser_success(r_parser_string, "foo", "X", NULL, "foo");
  assert_parser_success(r_parser_string, "foo=bar", "=", NULL, "foo=bar");
}

void
test_string(void)
{
  PARSER_TESTCASE(test_string_parser_without_parameter_parses_a_word);
  PARSER_TESTCASE(test_string_parser_with_additional_end_characters);
}

static gpointer
compile_qstring_state(const gchar *quotes)
{
  union
  {
    gpointer ptr;
    gchar ending_char;
  } state;

  memset(&state, 0, sizeof(state));
  state.ending_char = quotes[1];
  return state.ptr;
}

static void
assert_qstring_parser_success(const gchar *str, gchar *quotes, const gchar *expected_string)
{
  assert_parser_success(r_parser_qstring, str, quotes, compile_qstring_state(quotes), expected_string);
}

static void
test_qstring_parser_extracts_word_from_quotes(void)
{
  gchar *single_quotes = "''";
  gchar *double_quotes = "\"\"";
  gchar *braces = "{}";

  assert_qstring_parser_success("'foo'", single_quotes, "foo");
  assert_qstring_parser_success("\"foo\"", double_quotes, "foo");
  assert_qstring_parser_success("{foo}", braces, "foo");
}

void
test_qstring(void)
{
  PARSER_TESTCASE(test_qstring_parser_extracts_word_from_quotes);
}

/* NOTE: incomplete, the rest of the testcases are missing */

int
main(void)
{
  test_string();
  test_qstring();
}
