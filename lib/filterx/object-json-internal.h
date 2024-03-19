/*
 * Copyright (c) 2023 Balazs Scheidler <balazs.scheidler@axoflow.com>
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
#ifndef OBJECT_JSON_INTERNAL_H_INCLUDED
#define OBJECT_JSON_INTERNAL_H_INCLUDED

#include "object-json.h"
#include "filterx/filterx-weakrefs.h"

void filterx_json_associate_cached_object(struct json_object *json_obj, FilterXObject *filterx_object);
FilterXObject *filterx_json_convert_json_to_object_cached(FilterXObject *self, FilterXWeakRef *root_container,
                                                          struct json_object *json_obj);

struct json_object *filterx_json_deep_copy(struct json_object *json_obj);

FilterXObject *filterx_json_object_new_sub(struct json_object *json_obj, FilterXObject *root);
FilterXObject *filterx_json_array_new_sub(struct json_object *json_obj, FilterXObject *root);

#endif
