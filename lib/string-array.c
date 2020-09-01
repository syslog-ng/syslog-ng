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

#include "string-array.h"

struct _StringArray
{
  GPtrArray *array;
};

StringArray *
string_array_new(guint initial_size)
{
  if (initial_size == 0)
    initial_size = 1;

  StringArray *self = g_new0(StringArray, 1);
  self->array = g_ptr_array_sized_new(initial_size);
  g_ptr_array_set_free_func(self->array, g_free);

  return self;
}

void
string_array_free(StringArray *self)
{
  g_ptr_array_unref(self->array);
  g_free(self);
}

gchar *
string_array_element_at(StringArray *self, guint idx)
{
  g_assert(idx < self->array->len);

  return g_ptr_array_index(self->array, idx);
}

void
string_array_add(StringArray *self, gchar *str)
{
  g_ptr_array_add(self->array, str);
}

guint
string_array_len(StringArray *self)
{
  return self->array->len;
}

void
string_array_foreach(StringArray *self, StringArrayFunc func, gpointer user_data)
{
  g_ptr_array_foreach(self->array, (GFunc) func, user_data);
}

