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
#ifndef FILTERX_EXPR_JSON_H_INCLUDED
#define FILTERX_EXPR_JSON_H_INCLUDED

#include "filterx/filterx-expr.h"

typedef struct _FilterXKeyValue FilterXKeyValue;

FilterXKeyValue *filterx_kv_new(const gchar *key, FilterXExpr *value_expr);
void filterx_kv_free(FilterXKeyValue *self);

FilterXExpr *filterx_dict_expr_new(FilterXExpr *fillable, GList *key_values);
FilterXExpr *filterx_dict_expr_inner_new(FilterXExpr *fillable, GList *key_values);


#endif
