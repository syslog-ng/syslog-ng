/*
 * Copyright (c) 2023 shifter
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
 *
 */

#ifndef OBJECT_OTEL_H
#define OBJECT_OTEL_H

#include "syslog-ng.h"

#include "compat/cpp-start.h"
#include "filterx/filterx-object.h"
#include "plugin.h"

gpointer grpc_otel_filterx_logrecord_contruct_new(Plugin *self);
FilterXObject *filterx_otel_logrecord_new_from_args(GPtrArray *args);

gpointer grpc_otel_filterx_resource_construct_new(Plugin *self);
FilterXObject *filterx_otel_resource_new_from_args(GPtrArray *args);

gpointer grpc_otel_filterx_scope_construct_new(Plugin *self);
FilterXObject *filterx_otel_scope_new_from_args(GPtrArray *args);

gpointer grpc_otel_filterx_kvlist_construct_new(Plugin *self);
FilterXObject *filterx_otel_kvlist_new_from_args(GPtrArray *args);

static inline FilterXObject *
filterx_otel_kvlist_new(void)
{
  return filterx_otel_kvlist_new_from_args(NULL);
}

gpointer grpc_otel_filterx_array_construct_new(Plugin *self);
FilterXObject *filterx_otel_array_new_from_args(GPtrArray *args);

static inline FilterXObject *
filterx_otel_array_new(void)
{
  return filterx_otel_array_new_from_args(NULL);
}

gpointer grpc_otel_filterx_enum_construct(Plugin *self);

FILTERX_DECLARE_TYPE(otel_logrecord);
FILTERX_DECLARE_TYPE(otel_resource);
FILTERX_DECLARE_TYPE(otel_scope);
FILTERX_DECLARE_TYPE(otel_kvlist);
FILTERX_DECLARE_TYPE(otel_array);

static inline void
otel_filterx_objects_global_init(void)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      filterx_type_init(&FILTERX_TYPE_NAME(otel_logrecord));
      filterx_type_init(&FILTERX_TYPE_NAME(otel_resource));
      filterx_type_init(&FILTERX_TYPE_NAME(otel_scope));
      filterx_type_init(&FILTERX_TYPE_NAME(otel_kvlist));
      filterx_type_init(&FILTERX_TYPE_NAME(otel_array));
      initialized = TRUE;
    }
}

#include "compat/cpp-end.h"

#endif
