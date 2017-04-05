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
#include "str-repr/encode.h"
#include "testutils.h"
#include "stopwatch.h"

#define str_repr_encode_testcase_begin(func, args)             \
  do                                                            \
    {                                                           \
      testcase_begin("%s(%s)", func, args);                     \
    }                                                           \
  while (0)

#define str_repr_encode_testcase_end()                           \
  do                                                            \
    {                                                           \
      testcase_end();                                           \
    }                                                           \
  while (0)

#define STR_REPR_ENCODE_TESTCASE(x, ...) \
  do {                                                          \
      str_repr_encode_testcase_begin(#x, #__VA_ARGS__);     \
      x(__VA_ARGS__);                                           \
      str_repr_encode_testcase_end();                                \
  } while(0)


GString *value;

static void
assert_encode_equals(const gchar *input, const gchar *expected)
{
  GString *str = g_string_new("");

  str_repr_encode(str, input, -1, NULL);
  assert_string(str->str, expected, "Encoded value does not match expected");

  str_repr_encode(str, input, strlen(input), NULL);
  assert_string(str->str, expected, "Encoded value does not match expected");

  gchar *space_ended_input = g_strdup_printf("%s ", input);
  str_repr_encode(str, space_ended_input, strlen(input), ",");
  assert_string(str->str, expected, "Encoded value does not match expected");

  g_free(space_ended_input);

  g_string_free(str, TRUE);
}

static void
assert_encode_with_forbidden_equals(const gchar *input, const gchar *forbidden_chars, const gchar *expected)
{
  GString *str = g_string_new("");

  str_repr_encode(str, input, -1, forbidden_chars);
  assert_string(str->str, expected, "Encoded value does not match expected");
  g_string_free(str, TRUE);
}

static void
test_encode_simple_strings(void)
{
  assert_encode_equals("", "\"\"");
  assert_encode_equals("a", "a");
  assert_encode_equals("alma", "alma");
  assert_encode_equals("al\nma", "\"al\\nma\"");
}

static void
test_encode_strings_that_need_quotation(void)
{
  assert_encode_equals("foo bar", "\"foo bar\"");
  /* embedded quote */
  assert_encode_equals("\"value1", "'\"value1'");
  assert_encode_equals("'value1", "\"'value1\"");
  /* control sequences */
  assert_encode_equals("\b \f \n \r \t \\", "\"\\b \\f \\n \\r \\t \\\\\"");
}

static void
test_encode_strings_with_forbidden_chars(void)
{
  assert_encode_with_forbidden_equals("foo,", ",", "\"foo,\"");
  assert_encode_with_forbidden_equals("\"'foo,", ",", "\"\\\"'foo,\"");
}
#define ITERATION_NUMBER 100000

static void
_perftest(const gchar *value_to_encode)
{
  gint iteration_index = 0;
  GString *result = g_string_sized_new(64);
  gsize value_len = strlen(value_to_encode);

  start_stopwatch();
  for (iteration_index = 0; iteration_index < ITERATION_NUMBER; iteration_index++)
    {
      str_repr_encode(result, value_to_encode, value_len, ",");
    }
  stop_stopwatch_and_display_result(iteration_index, "%.64s...", value_to_encode);
  g_string_free(result, TRUE);
}

static void
test_performance(void)
{
  _perftest("This is a long value with spaces and control characters\n"
            "                                                         ");
  _perftest("This is 'a long' value with spaces and control characters\n");
  _perftest("This is \"a long\" value with spaces and control characters\n");
}

int
main(int argc, char *argv[])
{
  STR_REPR_ENCODE_TESTCASE(test_encode_simple_strings);
  STR_REPR_ENCODE_TESTCASE(test_encode_strings_that_need_quotation);
  STR_REPR_ENCODE_TESTCASE(test_encode_strings_with_forbidden_chars);
  STR_REPR_ENCODE_TESTCASE(test_performance);
  return 0;
}
