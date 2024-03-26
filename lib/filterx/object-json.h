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
#ifndef OBJECT_JSON_H_INCLUDED
#define OBJECT_JSON_H_INCLUDED

#include "filterx/filterx-object.h"
#include "compat/json.h"

typedef struct FilterXJsonObject_ FilterXJsonObject;
typedef struct FilterXJsonArray_ FilterXJsonArray;

FILTERX_DECLARE_TYPE(json_object);
FILTERX_DECLARE_TYPE(json_array);

FilterXObject *filterx_json_new_from_repr(const gchar *repr, gssize repr_len);
FilterXObject *filterx_json_object_new_from_repr(const gchar *repr, gssize repr_len);
FilterXObject *filterx_json_array_new_from_repr(const gchar *repr, gssize repr_len);
FilterXObject *filterx_json_array_new_from_syslog_ng_list(const gchar *repr, gssize repr_len);

FilterXObject *filterx_json_object_new_empty(void);
FilterXObject *filterx_json_array_new_empty(void);

FilterXObject *filterx_json_new_from_args(GPtrArray *args);
FilterXObject *filterx_json_array_new_from_args(GPtrArray *args);

const gchar *filterx_json_to_json_literal(FilterXObject *s);
const gchar *filterx_json_object_to_json_literal(FilterXObject *s);
const gchar *filterx_json_array_to_json_literal(FilterXObject *s);

#endif
