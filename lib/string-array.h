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

#ifndef STRING_ARRAY_H_INCLUDED
#define STRING_ARRAY_H_INCLUDED

#include "syslog-ng.h"

typedef struct _StringArray StringArray;
typedef void (*StringArrayFunc)(const gchar *str, gpointer user_data);

StringArray *string_array_new(guint initial_size);
void string_array_free(StringArray *self);

gchar *string_array_element_at(StringArray *self, guint idx);
void string_array_add(StringArray *self, gchar *str);
guint string_array_len(StringArray *self);
void string_array_foreach(StringArray *self, StringArrayFunc func, gpointer user_data);

#endif
