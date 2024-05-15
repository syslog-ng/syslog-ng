/*
 * Copyright (c) 2024 shifter <shifter@axoflow.com>
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

#include <criterion/criterion.h>
#include "libtest/cr_template.h"
#include "libtest/filterx-lib.h"

#include "filterx/func-istype.h"
#include "filterx/filterx-object.h"
#include "filterx/expr-literal.h"
#include "filterx/object-string.h"
#include "filterx/object-primitive.h"
#include "filterx/object-list-interface.h"
#include "filterx/expr-get-subscript.h"

#include "apphook.h"

FILTERX_DECLARE_TYPE(dummy_base);
FILTERX_DECLARE_TYPE(dummy);
FILTERX_DEFINE_TYPE(dummy_base, FILTERX_TYPE_NAME(object));
FILTERX_DEFINE_TYPE(dummy, FILTERX_TYPE_NAME(dummy_base));

Test(filterx_func_istype, null_args)
{
  cr_assert_not(filterx_function_istype_new("istype", filterx_function_args_new(NULL, NULL), NULL));
}

Test(filterx_func_istype, null_object_arg)
{
  GList *args = NULL;
  args = g_list_append(args, filterx_function_arg_new(NULL, NULL));
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_string_new("json_object", -1))));

  cr_assert_not(filterx_function_istype_new("istype", filterx_function_args_new(args, NULL), NULL));
}

Test(filterx_func_istype, null_type_arg)
{
  GList *args = NULL;
  args = g_list_append(args, filterx_function_arg_new(NULL,
                                                      filterx_literal_new(filterx_object_new(&FILTERX_TYPE_NAME(dummy)))));
  args = g_list_append(args, filterx_function_arg_new(NULL, NULL));

  cr_assert_not(filterx_function_istype_new("istype", filterx_function_args_new(args, NULL), NULL));
}

Test(filterx_func_istype, invalid_type_arg)
{
  GList *args = NULL;
  args = g_list_append(args, filterx_function_arg_new(NULL,
                                                      filterx_literal_new(filterx_object_new(&FILTERX_TYPE_NAME(dummy)))));
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_integer_new(33))));

  cr_assert_not(filterx_function_istype_new("istype", filterx_function_args_new(args, NULL), NULL));
}

Test(filterx_func_istype, too_many_args)
{
  GList *args = NULL;
  args = g_list_append(args, filterx_function_arg_new(NULL,
                                                      filterx_literal_new(filterx_object_new(&FILTERX_TYPE_NAME(dummy)))));
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_string_new("dummy", -1))));
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_string_new("dummy_base", -1))));

  cr_assert_not(filterx_function_istype_new("istype", filterx_function_args_new(args, NULL), NULL));
}

Test(filterx_func_istype, non_literal_type_arg)
{
  FilterXObject *list = filterx_test_list_new();
  FilterXObject *type_str = filterx_string_new("dummy", -1);
  filterx_list_append(list, &type_str);
  filterx_object_unref(type_str);

  FilterXExpr *type_expr = filterx_get_subscript_new(filterx_literal_new(list),
                                                     filterx_literal_new(filterx_integer_new(-1)));

  GList *args = NULL;
  args = g_list_append(args, filterx_function_arg_new(NULL,
                                                      filterx_literal_new(filterx_object_new(&FILTERX_TYPE_NAME(dummy)))));
  args = g_list_append(args, filterx_function_arg_new(NULL, type_expr));

  cr_assert_not(filterx_function_istype_new("istype", filterx_function_args_new(args, NULL), NULL));
}

Test(filterx_func_istype, non_matching_type)
{
  GList *args = NULL;
  args = g_list_append(args, filterx_function_arg_new(NULL,
                                                      filterx_literal_new(filterx_object_new(&FILTERX_TYPE_NAME(dummy)))));
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_string_new("string", -1))));

  FilterXFunction *func_expr = filterx_function_istype_new("istype", filterx_function_args_new(args, NULL), NULL);
  cr_assert(func_expr);

  FilterXObject *result_obj = filterx_expr_eval(&func_expr->super);
  cr_assert(result_obj);

  gboolean result;
  cr_assert(filterx_boolean_unwrap(result_obj, &result));
  cr_assert_not(result);

  filterx_object_unref(result_obj);
  filterx_expr_unref(&func_expr->super);
}

Test(filterx_func_istype, matching_type)
{
  GList *args = NULL;
  args = g_list_append(args, filterx_function_arg_new(NULL,
                                                      filterx_literal_new(filterx_object_new(&FILTERX_TYPE_NAME(dummy)))));
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_string_new("dummy", -1))));

  FilterXFunction *func_expr = filterx_function_istype_new("istype", filterx_function_args_new(args, NULL), NULL);
  cr_assert(func_expr);

  FilterXObject *result_obj = filterx_expr_eval(&func_expr->super);
  cr_assert(result_obj);

  gboolean result;
  cr_assert(filterx_boolean_unwrap(result_obj, &result));
  cr_assert(result);

  filterx_object_unref(result_obj);
  filterx_expr_unref(&func_expr->super);
}

Test(filterx_func_istype, matching_type_for_super_type)
{
  GList *args = NULL;
  args = g_list_append(args, filterx_function_arg_new(NULL,
                                                      filterx_literal_new(filterx_object_new(&FILTERX_TYPE_NAME(dummy)))));
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_string_new("dummy_base", -1))));

  FilterXFunction *func_expr = filterx_function_istype_new("istype", filterx_function_args_new(args, NULL), NULL);
  cr_assert(func_expr);

  FilterXObject *result_obj = filterx_expr_eval(&func_expr->super);
  cr_assert(result_obj);

  gboolean result;
  cr_assert(filterx_boolean_unwrap(result_obj, &result));
  cr_assert(result);

  filterx_object_unref(result_obj);
  filterx_expr_unref(&func_expr->super);
}

Test(filterx_func_istype, matching_type_for_root_type)
{
  GList *args = NULL;
  args = g_list_append(args, filterx_function_arg_new(NULL,
                                                      filterx_literal_new(filterx_object_new(&FILTERX_TYPE_NAME(dummy)))));
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_string_new("object", -1))));

  FilterXFunction *func_expr = filterx_function_istype_new("istype", filterx_function_args_new(args, NULL), NULL);
  cr_assert(func_expr);

  FilterXObject *result_obj = filterx_expr_eval(&func_expr->super);
  cr_assert(result_obj);

  gboolean result;
  cr_assert(filterx_boolean_unwrap(result_obj, &result));
  cr_assert(result);

  filterx_object_unref(result_obj);
  filterx_expr_unref(&func_expr->super);
}

static void
setup(void)
{
  app_startup();
  init_libtest_filterx();
  filterx_type_init(&FILTERX_TYPE_NAME(dummy_base));
  filterx_type_init(&FILTERX_TYPE_NAME(dummy));
}

static void
teardown(void)
{
  deinit_libtest_filterx();
  app_shutdown();
}

TestSuite(filterx_func_istype, .init = setup, .fini = teardown);
