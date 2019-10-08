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
#include "string-list.h"
#include <criterion/criterion.h>

Test(string_list,test_string_array_to_list_converts_to_an_equivalent_glist)
{
  const gchar *arr[] =
  {
    "foo",
    "bar",
    "baz",
    NULL
  };
  GList *l;

  l = string_array_to_list(arr);
  cr_assert_eq(g_list_length(l), 3, "converted list is not the expected length");
  cr_assert_str_eq(l->data, "foo", "first element is expected to be foo");
  cr_assert_str_eq(l->next->data, "bar", "second element is expected to be bar");
  cr_assert_str_eq(l->next->next->data, "baz", "third element is expected to be baz");
  string_list_free(l);
}

Test(string_list,test_string_varargs_to_list_converts_to_an_equivalent_glist)
{
  GList *l;

  l = string_vargs_to_list("foo", "bar", "baz", NULL);
  cr_assert_eq(g_list_length(l), 3, "converted list is not the expected length");
  cr_assert_str_eq(l->data, "foo", "first element is expected to be foo");
  cr_assert_str_eq(l->next->data, "bar", "second element is expected to be bar");
  cr_assert_str_eq(l->next->next->data, "baz", "third element is expected to be baz");
  string_list_free(l);
}

Test(string_list,test_clone_string_array_duplicates_elements_while_leaving_token_values_intact)
{
  const gchar *arr[] =
  {
    "foo",
    "bar",
    "baz",
    NULL
  };
  GList *l, *l2;

  l = string_array_to_list(arr);
  l = g_list_append(l, GUINT_TO_POINTER(1));
  l = g_list_append(l, GUINT_TO_POINTER(2));
  l2 = string_list_clone(l);
  string_list_free(l);

  cr_assert_eq(g_list_length(l2), 5, "converted list is not the expected length");
  cr_assert_str_eq(l2->data, "foo", "first element is expected to be foo");
  cr_assert_str_eq(l2->next->data, "bar", "second element is expected to be bar");
  cr_assert_str_eq(l2->next->next->data, "baz", "third element is expected to be baz");
  cr_assert_eq(GPOINTER_TO_UINT(l2->next->next->next->data), 1,
               "fourth element is expected to be a token, with a value of 1");
  cr_assert_eq(GPOINTER_TO_UINT(l2->next->next->next->next->data), 2,
               "fifth element is expected to be a token, with a value of 2");

  string_list_free(l2);
}
