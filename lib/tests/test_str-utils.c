/*
 * Copyright (c) 2018 Balabit
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
#include <criterion/criterion.h>
#include <criterion/parameterized.h>
#include "str-utils.h"

/* this macro defines which strchr() implementation we are testing. It is
 * extracted as a macro in order to make it easy to switch strchr()
 * implementation, provided there would be more.
 */

#define strchr_under_test _strchr_optimized_for_single_char_haystack

typedef struct _StrChrTestData
{
  gchar *str;
  int c;
  int ofs;
} StrChrTestData;

ParameterizedTestParameters(str_utils, test_str_utils_is_null)
{
  static StrChrTestData test_data_list[] =
  {
    {"", 'x', 0},
    {"\0x", 'x', 0},
    {"a", 'x', 0},
    {"abc", 'x', 0}
  };

  return cr_make_param_array(StrChrTestData, test_data_list, sizeof(test_data_list) / sizeof(test_data_list[0]));
}

ParameterizedTest(StrChrTestData *test_data, str_utils, test_str_utils_is_null)
{
  cr_assert_null(strchr_under_test(test_data->str, test_data->c), "expected a NULL return");
}

ParameterizedTestParameters(str_utils, test_str_utils_find_char)
{
  static StrChrTestData test_data_list[] =
  {
    {"", '\0', 0},
    {"a", 'a', 0},
    {"a", '\0', 1},
    {"abc", 'a', 0},
    {"abc", 'b', 1},
    {"abc", 'c', 2},
    {"abc", '\0', 3},
    {"0123456789abcdef", '0', 0},
    {"0123456789abcdef", '7', 7},
    {"0123456789abcdef", 'f', 15}
  };

  return cr_make_param_array(StrChrTestData, test_data_list, sizeof(test_data_list) / sizeof(test_data_list[0]));
}

ParameterizedTest(StrChrTestData *test_data, str_utils, test_str_utils_find_char)
{
  char *result = strchr_under_test(test_data->str, test_data->c);

  cr_assert_not_null(result, "expected a non-NULL return");
  cr_assert(result - test_data->str <= strlen(test_data->str),
            "Expected the strchr() return value to point into the input string or the terminating NUL, it points past the NUL");
  cr_assert(result >= test_data->str,
            "Expected the strchr() return value to point into the input string or the terminating NUL, it points before the start of the string");
  cr_assert_eq((result - test_data->str), test_data->ofs,
               "Expected the strchr() return value to point right to the specified offset");
}

Test(strsplit, when_tokens_not_limited_find_all_tokens)
{
  gchar **tokens = strsplit("  ABB   CCC DDDD          111", ' ', 0);

  gint tokens_n = g_strv_length(tokens);
  cr_expect_eq(tokens_n, 4);
  cr_expect_str_eq(tokens[0], "ABB");
  cr_expect_str_eq(tokens[1], "CCC");
  cr_expect_str_eq(tokens[2], "DDDD");
  cr_expect_str_eq(tokens[3], "111");
  cr_expect_null(tokens[4]);

  g_strfreev(tokens);
}

Test(strsplit, when_string_without_delim_return_the_string)
{
  gchar **tokens = strsplit("111", ' ', 0);

  gint tokens_n = g_strv_length(tokens);
  cr_expect_eq(tokens_n, 1);

  cr_expect_str_eq(tokens[0], "111");
  cr_expect_null(tokens[1]);

  g_strfreev(tokens);
}

Test(strsplit, when_empty_string_return_the_empty_string)
{
  gchar **tokens = strsplit("", ' ', 0);
  gint tokens_n = g_strv_length(tokens);
  cr_expect_eq(tokens_n, 1);

  cr_expect_str_eq(tokens[0], "");
  cr_expect_null(tokens[1]);

  g_strfreev(tokens);
}

Test(strsplit, when_null_str_or_null_delim_return_null)
{
  gchar **tokens = strsplit(NULL, ' ', 0);
  cr_expect_null(tokens);

  tokens = strsplit(NULL, '\0', 0);
  cr_expect_null(tokens);

  tokens = strsplit("AAA ", '\0', 0);
  cr_expect_null(tokens);
}

Test(strsplit, when_tokens_limited_join_remaining_tokens)
{
  gchar **tokens = strsplit("  ABB   CCC DDDD          111", ' ', 3);

  gint tokens_n = g_strv_length(tokens);
  cr_expect_eq(tokens_n, 3);
  cr_expect_str_eq(tokens[0], "ABB");
  cr_expect_str_eq(tokens[1], "CCC");
  cr_expect_str_eq(tokens[2], "DDDD          111");
  cr_expect_null(tokens[3]);

  g_strfreev(tokens);
}
