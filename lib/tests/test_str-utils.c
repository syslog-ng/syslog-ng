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
