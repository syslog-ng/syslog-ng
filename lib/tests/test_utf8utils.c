/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2015 Balázs Scheidler
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

#include "utf8utils.h"
#include <criterion/criterion.h>
#include <criterion/parameterized.h>

typedef struct _StringValueList
{
  const gchar *str;
  const gchar *expected_escaped_str;
  const gchar *unsafe_chars;
  gssize str_len;
} StringValueList;


ParameterizedTestParameters(test_utf8utils, test_escaped_binary)
{
  static StringValueList string_value_list[] =
  {
    {"", "", NULL, -1},
    {"\n", "\\n", NULL, -1},
    {"\b \f \n \r \t", "\\b \\f \\n \\r \\t", NULL, -1},
    {"\\", "\\\\", NULL, -1},
    {"árvíztűrőtükörfúrógép", "árvíztűrőtükörfúrógép", NULL, -1},
    {"árvíztűrőtükörfúrógép\n", "árvíztűrőtükörfúrógép\\n", NULL, -1},
    {"\x41", "A", NULL, -1},
    {"\x7", "\\x07", NULL, -1},
    {"\xad", "\\xad", NULL, -1},
    {"Á\xadÉ", "Á\\xadÉ", NULL, -1},
    {"\xc3\x00\xc1""", "\\xc3", NULL, -1},
    {"\"text\"", "\\\"text\\\"", "\"", -1},
    {"\"text\"", "\\\"te\\xt\\\"", "\"x", -1},
    {"\xc3""\xa1 non zero terminated", "\\xc3", NULL, 1},
    {"\xc3""\xa1 non zero terminated", "á", NULL, 2},
  };

  return cr_make_param_array(StringValueList, string_value_list,
                             sizeof(string_value_list) / sizeof(string_value_list[0]));
}

ParameterizedTest(StringValueList *string_value_list, test_utf8utils, test_escaped_binary)
{
  GString *escaped_str = g_string_sized_new(64);

  append_unsafe_utf8_as_escaped_binary(escaped_str, string_value_list->str, string_value_list->str_len,
                                       string_value_list->unsafe_chars);

  cr_assert_str_eq(escaped_str->str, string_value_list->expected_escaped_str, "Escaped UTF-8 string is not as expected");
  g_string_free(escaped_str, TRUE);
}


ParameterizedTestParameters(test_utf8utils, test_escaped_text)
{
  static StringValueList string_value_list[] =
  {
    {"", "", NULL, -1},
    {"\n", "\\n", NULL, -1},
    {"\b \f \n \r \t", "\\b \\f \\n \\r \\t", NULL, -1},
    {"\\", "\\\\", NULL, -1},
    {"árvíztűrőtükörfúrógép", "árvíztűrőtükörfúrógép", NULL, -1},
    {"árvíztűrőtükörfúrógép\n", "árvíztűrőtükörfúrógép\\n", NULL, -1},
    {"\x41", "A", NULL, -1},
    {"\x7", "\\u0007", NULL, -1},
    {"\xad", "\\\\xad", NULL, -1},
    {"Á\xadÉ", "Á\\\\xadÉ", NULL, -1},
    {"\"text\"", "\\\"text\\\"", "\"", -1},
    {"\"text\"", "\\\"te\\xt\\\"", "\"x", -1},
  };

  return cr_make_param_array(StringValueList, string_value_list,
                             sizeof(string_value_list) / sizeof(string_value_list[0]));
}

ParameterizedTest(StringValueList *string_value_list, test_utf8utils, test_escaped_text)
{
  gchar *escaped_str = convert_unsafe_utf8_to_escaped_text(string_value_list->str, string_value_list->str_len,
                                                           string_value_list->unsafe_chars);

  cr_assert_str_eq(escaped_str, string_value_list->expected_escaped_str, "Escaped UTF-8 string is not as expected");
  g_free(escaped_str);
}
