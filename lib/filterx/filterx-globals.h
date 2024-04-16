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
#ifndef FILTERX_GLOBALS_H_INCLUDED
#define FILTERX_GLOBALS_H_INCLUDED

#include "filterx-object.h"
#include "filterx/expr-function.h"

void filterx_global_init(void);
void filterx_global_deinit(void);
// Builtin functions
FilterXFunctionProto filterx_builtin_function_lookup(const gchar *);
// FilterX types
FilterXType *filterx_type_lookup(const gchar *type_name);
gboolean filterx_type_register(const gchar *type_name, FilterXType *fxtype);
// Helpers
FilterXObject *filterx_typecast_get_arg(GPtrArray *args, gchar *alt_msg);

#endif
