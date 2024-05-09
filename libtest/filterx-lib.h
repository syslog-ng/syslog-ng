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

#ifndef LIBTEST_FILTERX_LIB_H_INCLUDED
#define LIBTEST_FILTERX_LIB_H_INCLUDED

#include "filterx/filterx-object.h"
#include "filterx/filterx-expr.h"

void assert_marshaled_object(FilterXObject *obj, const gchar *repr, LogMessageValueType type);
void assert_object_json_equals(FilterXObject *obj, const gchar *expected_json_repr);

FILTERX_DECLARE_TYPE(test_dict);
FILTERX_DECLARE_TYPE(test_list);
FILTERX_DECLARE_TYPE(test_unknown_object);

FilterXObject *filterx_test_dict_new(void);
FilterXObject *filterx_test_list_new(void);
FilterXObject *filterx_test_unknown_object_new(void);

const gchar *filterx_test_unknown_object_marshaled_repr(gssize *len);
const gchar *filterx_test_unknown_object_repr(gssize *len);

FilterXExpr *filterx_non_literal_new(FilterXObject *object);
FilterXExpr *filterx_error_expr_new(void);

void init_libtest_filterx(void);
void deinit_libtest_filterx(void);

#endif
