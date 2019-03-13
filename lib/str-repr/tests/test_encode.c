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
#include <criterion/criterion.h>
#include <criterion/parameterized.h>
#include "stopwatch.h"

GString *value;

typedef struct _EncodeTestStr
{
  const gchar *actual;
  const gchar *expected;
} EncodeTestStr;

typedef struct _EncodeTestForbidden
{
  const gchar *actual;
  const gchar *forbidden;
  const gchar *expected;
} EncodeTestForbidden;

static void
assert_encode_equals(const gchar *input, const gchar *expected)
{
  GString *str = g_string_new("");

  str_repr_encode(str, input, -1, NULL);
  cr_assert_str_eq(str->str, expected, "Encoded value does not match expected; actual: %s, expected: %s", str->str,
                   expected);

  str_repr_encode(str, input, strlen(input), NULL);
  cr_assert_str_eq(str->str, expected, "Encoded value does not match expected; actual: %s, expected: %s", str->str,
                   expected);

  gchar *space_ended_input = g_strdup_printf("%s ", input);
  str_repr_encode(str, space_ended_input, strlen(input), ",");
  cr_assert_str_eq(str->str, expected, "Encoded value does not match expected; actual: %s, expected: %s", str->str,
                   expected);

  g_free(space_ended_input);

  g_string_free(str, TRUE);
}

static void
assert_encode_with_forbidden_equals(const gchar *input, const gchar *forbidden_chars, const gchar *expected)
{
  GString *str = g_string_new("");

  str_repr_encode(str, input, -1, forbidden_chars);
  cr_assert_str_eq(str->str, expected, "Encoded value does not match expected; actual: %s, expected: %s", str->str,
                   expected);
  g_string_free(str, TRUE);
}

ParameterizedTestParameters(encode, test_strings)
{
  static EncodeTestStr test_cases[] =
  {
    {"", "\"\""},
    {"a", "a"},
    {"alma", "alma"},
    {"al\nma", "\"al\\nma\""},

    {"foo bar", "\"foo bar\""},
    /* embedded quote */
    {"\"value1", "'\"value1'"},
    {"'value1", "\"'value1\""},
    /* control sequences */
    {"\b \f \n \r \t \\", "\"\\b \\f \\n \\r \\t \\\\\""}
  };

  return cr_make_param_array(EncodeTestStr, test_cases, sizeof(test_cases) / sizeof(test_cases[0]));
}

ParameterizedTest(EncodeTestStr *test_cases, encode, test_strings)
{
  assert_encode_equals(test_cases->actual, test_cases->expected);
}

ParameterizedTestParameters(encode, test_encode_strings_that_need_quotation)
{
  static EncodeTestForbidden test_cases[] =
  {
    {"foo,", ",", "\"foo,\""},
    {"\"'foo,", ",", "\"\\\"'foo,\""}
  };

  return cr_make_param_array(EncodeTestForbidden, test_cases, sizeof(test_cases) / sizeof(test_cases[0]));
}

ParameterizedTest(EncodeTestForbidden *test_cases, encode, test_encode_strings_that_need_quotation)
{
  assert_encode_with_forbidden_equals(test_cases->actual, test_cases->forbidden, test_cases->expected);
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

Test(encode, test_performance)
{
  _perftest("This is a long value with spaces and control characters\n"
            "                                                         ");
  _perftest("This is 'a long' value with spaces and control characters\n");
  _perftest("This is \"a long\" value with spaces and control characters\n");
}
