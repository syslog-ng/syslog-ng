/*
 * Copyright (c) 2015-2016 Balabit
 * Copyright (c) 2015-2016 Balázs Scheidler
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
#include "str-repr/decode.h"
#include "testutils.h"

#define str_repr_decode_testcase_begin(func, args)             \
  do                                                            \
    {                                                           \
      testcase_begin("%s(%s)", func, args);                     \
    }                                                           \
  while (0)

#define str_repr_decode_testcase_end()                           \
  do                                                            \
    {                                                           \
      testcase_end();                                           \
    }                                                           \
  while (0)

#define STR_REPR_DECODE_TESTCASE(x, ...) \
  do {                                                          \
      str_repr_decode_testcase_begin(#x, #__VA_ARGS__);     \
      x(__VA_ARGS__);                                           \
      str_repr_decode_testcase_end();                                \
  } while(0)


GString *value;

static void
assert_decode_equals(const gchar *input, const gchar *expected)
{
  GString *str = g_string_new("");
  const gchar *end;

  assert_true(str_repr_decode(str, input, &end), "Decode operation failed while success was expected, input=%s", input);
  assert_string(str->str, expected, "Decoded value does not match expected");
  g_string_free(str, TRUE);
}

static void
assert_decode_equals_and_fails(const gchar *input, const gchar *expected)
{
  GString *str = g_string_new("");
  const gchar *end;

  assert_false(str_repr_decode(str, input, &end), "Decode operation succeeded while failure was expected, input=%s",
               input);
  assert_string(str->str, expected, "Decoded value does not match expected");
  g_string_free(str, TRUE);
}

static gboolean
_match_three_tabs_as_delimiter(const gchar *cur, const gchar **new_cur, gpointer user_data)
{
  *new_cur = cur + 3;
  return (strncmp(cur, "\t\t\t", 3) == 0);
}

static void
assert_decode_with_three_tabs_as_delimiter_equals(const gchar *input, const gchar *expected)
{
  StrReprDecodeOptions options =
  {
    .match_delimiter = _match_three_tabs_as_delimiter,
    0
  };
  GString *str = g_string_new("");
  const gchar *end;

  str_repr_decode_with_options(str, input, &end, &options);
  assert_string(str->str, expected, "Decoded value does not match expected");
  g_string_free(str, TRUE);
}

static void
test_decode_unquoted_strings(void)
{
  assert_decode_equals("a", "a");
  assert_decode_equals("alma", "alma");
  assert_decode_equals("al ma", "al");
}

static void
test_decode_double_quoted_strings(void)
{
  assert_decode_equals("\"al ma\"", "al ma");
  /* embedded quote */
  assert_decode_equals("\"\\\"value1\"", "\"value1");
  /* control sequences */
  assert_decode_equals("\"\\b \\f \\n \\r \\t \\\\\"", "\b \f \n \r \t \\");
  /* unknown backslash escape is left as is */
  assert_decode_equals("\"\\p\"", "\\p");
}

static void
test_decode_apostrophe_quoted_strings(void)
{
  assert_decode_equals("'al ma'", "al ma");
  /* embedded quote */
  assert_decode_equals("'\\'value1'", "'value1");

  /* control sequences */
  assert_decode_equals("'\\b \\f \\n \\r \\t \\\\'", "\b \f \n \r \t \\");
  /* unknown backslash escape is left as is */
  assert_decode_equals("'\\p\'", "\\p");
}

static void
test_decode_malformed_strings(void)
{
  assert_decode_equals_and_fails("'alma", "'alma");
  assert_decode_equals_and_fails("\"alma", "\"alma");
  assert_decode_equals("alma'", "alma'");
  assert_decode_equals("alma\"", "alma\"");
  assert_decode_equals("alma\"korte", "alma\"korte");
  assert_decode_equals("alma\"korte\"", "alma\"korte\"");
  assert_decode_equals_and_fails("'alma'@korte", "'alma'");
}

static void
test_decode_delimited_strings(void)
{
  assert_decode_with_three_tabs_as_delimiter_equals("alma\t\t\tkorte", "alma");

  assert_decode_with_three_tabs_as_delimiter_equals("'alma\t\t\tkorte'", "alma\t\t\tkorte");
  assert_decode_with_three_tabs_as_delimiter_equals("'alma\t\t\tkorte'\t\t", "'alma\t\t\tkorte'");
  assert_decode_with_three_tabs_as_delimiter_equals("'alma\t\t\tkorte'\t\t\t", "alma\t\t\tkorte");
  assert_decode_with_three_tabs_as_delimiter_equals("alma\t\t", "alma\t\t");

  assert_decode_with_three_tabs_as_delimiter_equals("\t\t\tfoobar", "");

}

int
main(int argc, char *argv[])
{
  STR_REPR_DECODE_TESTCASE(test_decode_unquoted_strings);
  STR_REPR_DECODE_TESTCASE(test_decode_double_quoted_strings);
  STR_REPR_DECODE_TESTCASE(test_decode_apostrophe_quoted_strings);
  STR_REPR_DECODE_TESTCASE(test_decode_malformed_strings);
  STR_REPR_DECODE_TESTCASE(test_decode_delimited_strings);
  return 0;
}
