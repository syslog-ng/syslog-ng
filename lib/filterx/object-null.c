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
#include "object-null.h"
#include "filterx-globals.h"

static FilterXObject *null_object;

static gboolean
_truthy(FilterXObject *s)
{
  return FALSE;
}

static gboolean
_marshal(FilterXObject *s, GString *repr, LogMessageValueType *t)
{
  *t = LM_VT_NULL;
  return TRUE;
}

static gboolean
_map_to_json(FilterXObject *s, struct json_object **object, FilterXObject **assoc_object)
{
  *object = NULL;
  return TRUE;
}

gboolean
null_repr(GString *repr)
{
  g_string_append_len(repr, "null", 4);
  return TRUE;
}

static gboolean
_null_repr(FilterXObject *s, GString *repr)
{
  return null_repr(repr);
}

FilterXObject *
_null_wrap(void)
{
  return filterx_object_new(&FILTERX_TYPE_NAME(null));
}

FilterXObject *
filterx_null_new(void)
{
  return filterx_object_ref(null_object);
}

FILTERX_DEFINE_TYPE(null, FILTERX_TYPE_NAME(object),
                    .map_to_json = _map_to_json,
                    .marshal = _marshal,
                    .repr = _null_repr,
                    .truthy = _truthy,
                   );

void
filterx_null_global_init(void)
{
  filterx_cache_object(&null_object, _null_wrap());
}

void
filterx_null_global_deinit(void)
{
  filterx_uncache_object(&null_object);
}
