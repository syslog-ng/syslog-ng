/*
 * Copyright (c) 2024 shifter <shifter@axoflow.com>
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

#include <criterion/criterion.h>
#include "libtest/cr_template.h"

#include "filterx/filterx-object.h"
#include "filterx/object-primitive.h"
#include "filterx/expr-comparison.h"
#include "filterx/expr-condition.h"
#include "filterx/filterx-expr.h"
#include "filterx/expr-literal.h"
#include "filterx/object-string.h"
#include "filterx/object-null.h"
#include "filterx/object-datetime.h"
#include "filterx/object-message-value.h"
#include "filterx/expr-assign.h"
#include "filterx/expr-template.h"
#include "filterx/expr-variable.h"
#include "filterx/expr-function.h"
#include "filterx/filterx-private.h"

#include "apphook.h"
#include "scratch-buffers.h"

FilterXObject *test_dummy_function(GPtrArray *args)
{
  GString *repr = scratch_buffers_alloc();
  GString *out = scratch_buffers_alloc();
  for (int i = 0; i < args->len; i++)
    {
      FilterXObject *object = g_ptr_array_index(args, i);
      cr_assert_not_null(object);
      cr_assert(filterx_object_repr(object, repr));
      g_string_append(out, repr->str);
    }

  return filterx_string_new(out->str, out->len);
}

Test(expr_function, test_function_null_args)
{
  FilterXExpr *func = filterx_simple_function_new("test_dummy", filterx_function_args_new(NULL, NULL),
                                                  test_dummy_function);
  cr_assert_not_null(func);
  filterx_expr_unref(func);
}

Test(expr_function, test_function_null_arg)
{
  GList *args = NULL;
  args = g_list_append(args, filterx_function_arg_new(NULL, NULL));
  FilterXExpr *func = filterx_simple_function_new("test_dummy", filterx_function_args_new(args, NULL),
                                                  test_dummy_function);
  cr_assert_not_null(func);
  filterx_expr_unref(func);
}

Test(expr_function, test_function_valid_arg)
{
  GList *args = NULL;
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_string_new("bad format 1", -1))));

  FilterXExpr *func = filterx_simple_function_new("test_dummy", filterx_function_args_new(args, NULL),
                                                  test_dummy_function);
  cr_assert_not_null(func);

  FilterXObject *res = filterx_expr_eval(func);
  cr_assert_not_null(res);
  cr_assert(filterx_object_is_type(res, &FILTERX_TYPE_NAME(string)));
  const gchar *str = filterx_string_get_value(res, NULL);
  cr_assert_str_eq(str, "bad format 1");
  filterx_expr_unref(func);
  filterx_object_unref(res);
}

Test(expr_function, test_function_multiple_args)
{
  GList *args = NULL;
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_null_new())));
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_integer_new(443))));
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_string_new("foobar", -1))));
  FilterXExpr *func = filterx_simple_function_new("test_dummy", filterx_function_args_new(args, NULL),
                                                  test_dummy_function);
  cr_assert_not_null(func);
  FilterXObject *res = filterx_expr_eval(func);
  cr_assert_not_null(res);
  cr_assert(filterx_object_is_type(res, &FILTERX_TYPE_NAME(string)));
  const gchar *str = filterx_string_get_value(res, NULL);
  cr_assert_str_eq(str, "null443foobar");
  filterx_expr_unref(func);
  filterx_object_unref(res);
}

static void
setup(void)
{
  app_startup();
}

static void
teardown(void)
{
  scratch_buffers_explicit_gc();
  app_shutdown();
}

TestSuite(expr_function, .init = setup, .fini = teardown);
