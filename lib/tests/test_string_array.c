/*
 * Copyright (c) 2020 One Identity
 * Copyright (c) 2020 Laszlo Budai
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
#include "string-array.h"

static void
_concatenate(GString *str, gpointer result)
{
  GString *result_str = (GString *) result;
  result = g_string_append(result_str, str->str);
}

Test(string_array, basic_tests)
{
  StringArray *str_array = string_array_new(3);
  cr_assert_not_null(str_array);

  const gchar *strings[] =
  {
    "str_0",
    "str_1",
    "str_2",
    "str_3"
  };

  const gchar *concatenated_str_expected = "str_0str_1str_2str_3";

  for (guint i = 0; i < 4; i++)
    {
      string_array_add(str_array, g_string_new(strings[i]));
      cr_expect_eq(string_array_len(str_array), i+1);
    }

  for (guint i = 0; i < 4; i++)
    {
      cr_expect_str_eq(string_array_element_at(str_array, i)->str, strings[i]);
    }

  GString *concatenated_str = g_string_new("");
  string_array_foreach(str_array, _concatenate, concatenated_str);
  cr_expect_str_eq(concatenated_str->str, concatenated_str_expected);

  g_string_free(concatenated_str, TRUE);
  string_array_free(str_array);
}

