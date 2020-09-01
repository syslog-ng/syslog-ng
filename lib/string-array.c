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

static void
_string_free(gpointer user_data)
{
  GString *str = (GString *) user_data;
  g_string_free(str, TRUE);
}

StringArray *
string_array_new(guint initial_size)
{
  if (initial_size == 0)
    initial_size = 1;

  StringArray *self = g_new0(StringArray, 1);
  self->array = g_ptr_array_sized_new(initial_size);
  g_ptr_array_set_free_func(self->array, _string_free);

  return self;
}

void
string_array_free(StringArray *self)
{
  g_ptr_array_unref(self->array);
  g_free(self);
}

GString *
string_array_element_at(StringArray *self, guint idx)
{
  g_assert(idx < self->array->len);

  return g_ptr_array_index(self->array, idx);
}

void
string_array_add(StringArray *self, GString *str)
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

static void
_len(GString *str, gpointer user_data)
{
  gsize *len = (gsize *) user_data;
  *len += str->len;
}

static gsize
_total_str_len(StringArray *self)
{
  gsize len = 0;
  string_array_foreach(self, _len, &len);

  return len;
}

static void
_join(GString *str, gpointer user_data)
{
  GString *joined_str = (GString *) user_data;
  g_string_append(joined_str, str->str);
}

GString *
string_array_join(StringArray *self, gboolean free_elements)
{
  gsize len = _total_str_len(self);
  GString *str = g_string_sized_new(len + 1);
  string_array_foreach(self, _join, str);
  if (free_elements)
    self->array = g_ptr_array_remove_range(self->array, 0, string_array_len(self));

  return str;
}

