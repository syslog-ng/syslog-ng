/*
 * Copyright (c) 2002-2019 Balabit
 * Copyright (c) 2019 Laszlo Budai <laszlo.budai@balabit.com>
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

#ifndef LIST_ADT_H_INCLUDED
#define LIST_ADT_H_INCLUDED

#include <syslog-ng.h>

typedef struct _List List;

typedef void (*list_foreach_fn)(gconstpointer list_data, gpointer user_data);

struct _List
{
  void (*append)(List *self, gconstpointer item);
  void (*foreach)(List *self, list_foreach_fn foreach_fn, gpointer user_data);
  gboolean (*is_empty)(List *self);
  void (*remove_all)(List *self);
  void (*free_fn)(List *self);
};

static inline void
list_append(List *self, gconstpointer item)
{
  g_assert(self->append);

  self->append(self, item);
}

static inline void
list_foreach(List *self, list_foreach_fn foreach_fn, gpointer user_data)
{
  g_assert(self->foreach);

  self->foreach(self, foreach_fn, user_data);
}

static inline gboolean
list_is_empty(List *self)
{
  g_assert(self->is_empty);

  return self->is_empty(self);
}

static inline void
list_remove_all(List *self)
{
  g_assert(self->remove_all);

  self->remove_all(self);
}

static inline void
list_free(List *self)
{
  if (self->free_fn)
    self->free_fn(self);

  g_free(self);
}

#endif

