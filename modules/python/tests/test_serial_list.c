/*
 * Copyright (c) 2019 Balabit
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
 */

#include "serial-list.h"
#include "string.h"
#include <criterion/criterion.h>

void setup(void)
{
}

void teardown(void)
{
}

TestSuite(serial_list, .init = setup, .fini = teardown);

static gboolean
is_same_string(guchar *data, gsize data_len, gpointer user_data)
{
  gchar *string_to_find = user_data;

  return strncmp((gchar *)data, string_to_find, data_len) == 0;
}

Test(serial_list, test_serial_list)
{
  guchar buffer[200];
  SerialList *self = serial_list_new(buffer, sizeof(buffer));

  cr_assert_eq(serial_list_find(self, is_same_string, "foo"), 0);
  SerialListHandle handle = serial_list_insert(self, (guchar *)"foo", strlen("foo"));
  cr_assert(handle);

  const guchar *data;
  gsize data_len;
  serial_list_get_data(self, handle, &data, &data_len);
  cr_assert_eq(data_len, strlen("foo"));
  cr_assert_eq(strncmp((gchar *)data, "foo", strlen("foo")), 0);

  serial_list_free(self);
}

Test(serial_list, test_no_free_space)
{
  guchar buffer[90];
  SerialList *self = serial_list_new(buffer, sizeof(buffer));
  cr_assert_eq(serial_list_insert(self, (guchar *)"123456789ABCDEFGH", sizeof("123456789ABCDEFGH")), 0);
  serial_list_free(self);
}

Test(serial_list, test_two_data)
{
  guchar buffer[400];
  SerialList *self = serial_list_new(buffer, sizeof(buffer));

  SerialListHandle handle1 = serial_list_insert(self, (guchar *)"foo1", sizeof("foo1"));
  cr_assert(handle1);
  SerialListHandle handle2 = serial_list_insert(self, (guchar *)"foo2", sizeof("foo2"));
  cr_assert(handle2);

  cr_assert_str_eq((gchar *)serial_list_get_data(self, handle1, NULL, NULL), "foo1");
  cr_assert_str_eq((gchar *)serial_list_get_data(self, handle2, NULL, NULL), "foo2");

  serial_list_free(self);
}

Test(serial_list, test_simple_remove)
{
  guchar buffer[400];
  SerialList *self = serial_list_new(buffer, sizeof(buffer));

  SerialListHandle handle = serial_list_insert(self, (guchar *)"foo", sizeof("foo"));
  cr_assert(handle);
  cr_assert(serial_list_find(self, is_same_string, "foo"));

  serial_list_remove(self, handle);
  cr_assert_eq(serial_list_find(self, is_same_string, "foo"), 0);

  handle = serial_list_insert(self, (guchar *)"foo", sizeof("foo"));
  cr_assert(handle);
  cr_assert(serial_list_find(self, is_same_string, "foo"));

  serial_list_free(self);
}

Test(serial_list, test_simple_update)
{
  guchar buffer[400];
  SerialList *self = serial_list_new(buffer, sizeof(buffer));

  SerialListHandle handle1 = serial_list_insert(self, (guchar *)"foobar", strlen("foobar"));
  cr_assert(handle1);
  SerialListHandle same_length = serial_list_update(self, handle1, (guchar *)"barfoo", strlen("barfoo"));
  cr_assert(same_length);
  cr_assert_eq(handle1, same_length);

  SerialListHandle handle2 = serial_list_update(self, handle1, (guchar *)"foo", strlen("foo"));
  cr_assert(handle2);
  cr_assert_eq(handle1, handle2);
  SerialListHandle handle3 = serial_list_update(self, handle1, (guchar *)"foobar", strlen("foobar"));
  cr_assert(handle3);
  cr_assert_eq(handle2, handle3);

  serial_list_free(self);
}

Test(serial_list, test_remove_then_reuse_free_space)
{
  guchar buffer[400];
  SerialList *self = serial_list_new(buffer, sizeof(buffer));

  SerialListHandle handle1 = serial_list_insert(self, (guchar *)"foobar", strlen("foobar"));
  cr_assert(handle1);
  serial_list_remove(self, handle1);
  SerialListHandle handle2 = serial_list_insert(self, (guchar *)"foobar", strlen("foobar"));
  cr_assert_eq(handle1, handle2);

  serial_list_free(self);
}
