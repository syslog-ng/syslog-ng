/*
 * Copyright (c) 2023 Balazs Scheidler <balazs.scheidler@axoflow.com>
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
#ifndef FILTERX_PRIMITIVE_H_INCLUDED
#define FILTERX_PRIMITIVE_H_INCLUDED

#include "filterx/filterx-object.h"
#include "generic-number.h"

FILTERX_DECLARE_TYPE(integer);
FILTERX_DECLARE_TYPE(double);
FILTERX_DECLARE_TYPE(boolean);

typedef struct _FilterXPrimitive
{
  FilterXObject super;
  GenericNumber value;
} FilterXPrimitive;

typedef struct _FilterXEnumDefinition
{
  const gchar *name;
  gint64 value;
} FilterXEnumDefinition;

FilterXObject *filterx_integer_new(gint64 value);
FilterXObject *filterx_double_new(gdouble value);
FilterXObject *filterx_boolean_new(gboolean value);
FilterXObject *filterx_enum_new(GlobalConfig *cfg, const gchar *namespace_name, const gchar *enum_name);
GenericNumber filterx_primitive_get_value(FilterXObject *s);

FilterXObject *filterx_typecast_boolean(GPtrArray *args);
FilterXObject *filterx_typecast_integer(GPtrArray *args);
FilterXObject *filterx_typecast_double(GPtrArray *args);

static inline gboolean
filterx_integer_unwrap(FilterXObject *s, gint64 *value)
{
  FilterXPrimitive *self = (FilterXPrimitive *) s;

  if (!filterx_object_is_type(s, &FILTERX_TYPE_NAME(integer)))
    return FALSE;
  *value = gn_as_int64(&self->value);
  return TRUE;
}

static inline gboolean
filterx_double_unwrap(FilterXObject *s, gdouble *value)
{
  FilterXPrimitive *self = (FilterXPrimitive *) s;

  if (!filterx_object_is_type(s, &FILTERX_TYPE_NAME(double)))
    return FALSE;
  *value = gn_as_double(&self->value);
  return TRUE;
}

static inline gboolean
filterx_boolean_unwrap(FilterXObject *s, gboolean *value)
{
  FilterXPrimitive *self = (FilterXPrimitive *) s;

  if (!filterx_object_is_type(s, &FILTERX_TYPE_NAME(boolean)))
    return FALSE;
  *value = !!gn_as_int64(&self->value);
  return TRUE;
}

#endif
