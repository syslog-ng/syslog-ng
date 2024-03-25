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
#ifndef FILTERX_OBJECT_STRING_H_INCLUDED
#define FILTERX_OBJECT_STRING_H_INCLUDED

#include "filterx-object.h"

FILTERX_DECLARE_TYPE(string);
FILTERX_DECLARE_TYPE(bytes);
FILTERX_DECLARE_TYPE(protobuf);

const gchar *filterx_string_get_value(FilterXObject *s, gsize *length);
const gchar *filterx_bytes_get_value(FilterXObject *s, gsize *length);
const gchar *filterx_protobuf_get_value(FilterXObject *s, gsize *length);
FilterXObject *filterx_typecast_string(GPtrArray *args);
FilterXObject *filterx_typecast_bytes(GPtrArray *args);
FilterXObject *filterx_typecast_protobuf(GPtrArray *args);

FilterXObject *filterx_string_new(const gchar *str, gssize str_len);
FilterXObject *filterx_bytes_new(const gchar *str, gssize str_len);
FilterXObject *filterx_protobuf_new(const gchar *str, gssize str_len);

#endif
