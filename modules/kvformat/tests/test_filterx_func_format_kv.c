/*
 * Copyright (c) 2024 Attila Szakacs
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
 */

#include <criterion/criterion.h>
#include "libtest/filterx-lib.h"

#include "filterx-func-format-kv.h"
#include "filterx/expr-literal.h"
#include "filterx/object-string.h"
#include "filterx/object-primitive.h"
#include "filterx/object-null.h"
#include "filterx/object-json.h"

#include "apphook.h"
#include "scratch-buffers.h"

static void
_assert_format_kv_init_fail(GList *args)
{
  GError *err = NULL;
  GError *args_err = NULL;
  FilterXFunction *func = filterx_function_format_kv_new("test", filterx_function_args_new(args, &args_err), &err);
  cr_assert(!func);
  cr_assert(err);
  g_error_free(err);
}

static void
_assert_format_kv(GList *args, const gchar *expected_output)
{
  GError *err = NULL;
  GError *args_err = NULL;
  FilterXFunction *func = filterx_function_format_kv_new("test", filterx_function_args_new(args, &args_err), &err);
  cr_assert(!err);

  FilterXObject *obj = filterx_expr_eval(&func->super);
  cr_assert(obj);

  const gchar *output = filterx_string_get_value(obj, NULL);
  cr_assert(output);

  cr_assert_str_eq(output, expected_output);

  filterx_object_unref(obj);
  filterx_expr_unref(&func->super);
}

Test(filterx_func_format_kv, test_invalid_args)
{
  GList *args = NULL;

  /* no args */
  _assert_format_kv_init_fail(NULL);

  /* empty value_separator */
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_test_dict_new())));
  args = g_list_append(args, filterx_function_arg_new("value_separator", filterx_literal_new(filterx_string_new("",
                                                      -1))));
  _assert_format_kv_init_fail(args);
  args = NULL;

  /* too long value_separator */
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_test_dict_new())));
  args = g_list_append(args, filterx_function_arg_new("value_separator", filterx_literal_new(filterx_string_new("->",
                                                      -1))));
  _assert_format_kv_init_fail(args);
  args = NULL;

  /* non-literal value_separator */
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_test_dict_new())));
  args = g_list_append(args, filterx_function_arg_new("value_separator", filterx_non_literal_new(filterx_string_new("=",
                                                      -1))));
  _assert_format_kv_init_fail(args);
  args = NULL;

  /* non-string value_separator */
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_test_dict_new())));
  args = g_list_append(args, filterx_function_arg_new("value_separator", filterx_literal_new(filterx_integer_new(42))));
  _assert_format_kv_init_fail(args);
  args = NULL;

  /* error value_separator */
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_test_dict_new())));
  args = g_list_append(args, filterx_function_arg_new("value_separator", filterx_error_expr_new()));
  _assert_format_kv_init_fail(args);
  args = NULL;

  /* empty pair_separator */
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_test_dict_new())));
  args = g_list_append(args, filterx_function_arg_new("pair_separator", filterx_literal_new(filterx_string_new("", -1))));
  _assert_format_kv_init_fail(args);
  args = NULL;

  /* non-literal pair_separator */
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_test_dict_new())));
  args = g_list_append(args, filterx_function_arg_new("pair_separator", filterx_non_literal_new(filterx_string_new("",
                                                      -1))));
  _assert_format_kv_init_fail(args);
  args = NULL;

  /* non-string pair_separator */
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_test_dict_new())));
  args = g_list_append(args, filterx_function_arg_new("pair_separator", filterx_literal_new(filterx_integer_new(42))));
  _assert_format_kv_init_fail(args);
  args = NULL;

  /* error pair_separator */
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_test_dict_new())));
  args = g_list_append(args, filterx_function_arg_new("pair_separator", filterx_error_expr_new()));
  _assert_format_kv_init_fail(args);
  args = NULL;

  /* too_many_args */
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_test_dict_new())));
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_string_new("=", -1))));
  _assert_format_kv_init_fail(args);
  args = NULL;
}

Test(filterx_func_format_kv, test_full)
{
  FilterXExpr *kvs = filterx_literal_new(filterx_json_object_new_from_repr("{\"foo\":\"bar\",\"bar\":\"baz\"}", -1));
  GList *args = NULL;

  args = g_list_append(args, filterx_function_arg_new(NULL, kvs));
  args = g_list_append(args, filterx_function_arg_new("value_separator", filterx_literal_new(filterx_string_new("@",
                                                      -1))));
  args = g_list_append(args, filterx_function_arg_new("pair_separator", filterx_literal_new(filterx_string_new(" | ",
                                                      -1))));
  _assert_format_kv(args, "foo@bar | bar@baz");
}

Test(filterx_func_format_kv, test_inner_dict_and_list_is_skipped)
{
  FilterXExpr *kvs = filterx_literal_new(
                       filterx_json_object_new_from_repr("{\"foo\":\"bar\",\"x\":{},\"y\":[],\"bar\":\"baz\"}", -1));
  GList *args = NULL;

  args = g_list_append(args, filterx_function_arg_new(NULL, kvs));
  _assert_format_kv(args, "foo=bar, bar=baz");
}

Test(filterx_func_format_kv, test_space_gets_double_quoted)
{
  FilterXExpr *kvs = filterx_literal_new(
                       filterx_json_object_new_from_repr("{\"foo\":\"bar\",\"bar\": \"almafa korte\\\"fa\"}", -1));
  GList *args = NULL;

  args = g_list_append(args, filterx_function_arg_new(NULL, kvs));
  _assert_format_kv(args, "foo=bar, bar=\"almafa korte\\\"fa\"");
}

static void
setup(void)
{
  app_startup();
  init_libtest_filterx();
}

static void
teardown(void)
{
  scratch_buffers_explicit_gc();
  deinit_libtest_filterx();
  app_shutdown();
}

TestSuite(filterx_func_format_kv, .init = setup, .fini = teardown);
