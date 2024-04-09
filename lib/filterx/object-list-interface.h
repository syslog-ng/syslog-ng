/*
 * Copyright (c) 2024 Attila Szakacs
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

#ifndef FILTERX_OBJECT_LIST_INTERFACE_H_INCLUDED
#define FILTERX_OBJECT_LIST_INTERFACE_H_INCLUDED

#include "filterx/filterx-object.h"

typedef struct FilterXList_ FilterXList;

struct FilterXList_
{
  FilterXObject super;

  FilterXObject *(*get_subscript)(FilterXList *s, guint64 index);
  gboolean (*set_subscript)(FilterXList *s, guint64 index, FilterXObject *new_value);
  gboolean (*append)(FilterXList *s, FilterXObject *new_value);
  gboolean (*unset_index)(FilterXList *s, guint64 index);
  guint64 (*len)(FilterXList *s);
};

guint64 filterx_list_len(FilterXObject *s);
FilterXObject *filterx_list_get_subscript(FilterXObject *s, gint64 index);
gboolean filterx_list_set_subscript(FilterXObject *s, gint64 index, FilterXObject *new_value);
gboolean filterx_list_append(FilterXObject *s, FilterXObject *new_value);
gboolean filterx_list_unset_index(FilterXObject *s, gint64 index);

void filterx_list_init_instance(FilterXList *self, FilterXType *type);

FILTERX_DECLARE_TYPE(list);

#endif
