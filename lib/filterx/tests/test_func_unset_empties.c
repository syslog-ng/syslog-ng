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

#include <criterion/criterion.h>
#include "libtest/filterx-lib.h"

#include "filterx/func-unset-empties.h"
#include "filterx/object-string.h"
#include "filterx/object-primitive.h"
#include "filterx/object-json.h"
#include "filterx/expr-literal.h"

#include "apphook.h"
#include "scratch-buffers.h"

static void
_assert_unset_empties_init_fail(GList *args)
{
  GError *err = NULL;
  GError *args_err = NULL;
  FilterXFunction *func = filterx_function_unset_empties_new("test", filterx_function_args_new(args, &args_err), &err);
  cr_assert(!func);
  cr_assert(err);
  g_error_free(err);
}

static void
_assert_unset_empties(GList *args, const gchar *expected_repr)
{
  FilterXExpr *modifiable_object_expr = filterx_expr_ref(((FilterXFunctionArg *) args->data)->value);

  GError *err = NULL;
  GError *args_err = NULL;
  FilterXFunction *func = filterx_function_unset_empties_new("test", filterx_function_args_new(args, &args_err), &err);
  cr_assert(!err);

  FilterXObject *obj = filterx_expr_eval(&func->super);
  cr_assert(obj);
  gboolean success;
  cr_assert(filterx_boolean_unwrap(obj, &success));
  cr_assert(success);

  FilterXObject *modifiable_object = filterx_expr_eval(modifiable_object_expr);
  cr_assert(modifiable_object);

  GString *repr = g_string_new(NULL);
  cr_assert(filterx_object_repr(modifiable_object, repr));
  cr_assert_str_eq(repr->str, expected_repr, "unset_empties() result is unexpected. Actual: %s Expected: %s", repr->str,
                   expected_repr);

  g_string_free(repr, TRUE);
  filterx_object_unref(obj);
  filterx_expr_unref(&func->super);
  filterx_object_unref(modifiable_object);
  filterx_expr_unref(modifiable_object_expr);
}

Test(filterx_func_unset_empties, invalid_args)
{
  const gchar *named_literal_bool_arg_names[] = {"recursive", "string", "number", "null", "dict", "list"};
  GList *args = NULL;

  /* no args */
  _assert_unset_empties_init_fail(NULL);

  /* non-literal named arg */
  for (gint i = 0; i < G_N_ELEMENTS(named_literal_bool_arg_names); i++)
    {
      args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_test_dict_new())));
      args = g_list_append(args, filterx_function_arg_new(named_literal_bool_arg_names[i],
                                                          filterx_non_literal_new(filterx_boolean_new(TRUE))));
      _assert_unset_empties_init_fail(args);
      args = NULL;
    }

  /* non-boolean named arg */
  for (gint i = 0; i < G_N_ELEMENTS(named_literal_bool_arg_names); i++)
    {
      args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_test_dict_new())));
      args = g_list_append(args, filterx_function_arg_new(named_literal_bool_arg_names[i],
                                                          filterx_literal_new(filterx_string_new("", -1))));
      _assert_unset_empties_init_fail(args);
      args = NULL;
    }

  /* too many args */
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_test_dict_new())));
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_string_new("", -1))));
  _assert_unset_empties_init_fail(args);
  args = NULL;
}

static void
_test_type_modifier(const gchar *option_name, const gchar *value)
{
  GString *buffer = g_string_new(NULL);
  GList *args = NULL;

  /* dict */

  /* default is always true */
  g_string_printf(buffer, "{\"foo\":%s}", value);
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_json_new_from_repr(buffer->str,
                                                      buffer->len))));
  _assert_unset_empties(args, "{}");
  args = NULL;

  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_json_new_from_repr(buffer->str,
                                                      buffer->len))));
  args = g_list_append(args, filterx_function_arg_new(option_name, filterx_literal_new(filterx_boolean_new(FALSE))));
  _assert_unset_empties(args, buffer->str);
  args = NULL;

  /* list */

  /* default is always true */
  g_string_printf(buffer, "[%s]", value);
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_json_new_from_repr(buffer->str,
                                                      buffer->len))));
  _assert_unset_empties(args, "[]");
  args = NULL;

  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_json_new_from_repr(buffer->str,
                                                      buffer->len))));
  args = g_list_append(args, filterx_function_arg_new(option_name, filterx_literal_new(filterx_boolean_new(FALSE))));
  _assert_unset_empties(args, buffer->str);
  args = NULL;

  g_string_free(buffer, TRUE);
}

Test(filterx_func_unset_empties, type_modifiers)
{
  _test_type_modifier("string", "\"\"");
  _test_type_modifier("number", "0");
  _test_type_modifier("number", "0.0");
  _test_type_modifier("null", "null");
  _test_type_modifier("dict", "{}");
  _test_type_modifier("list", "[]");
}

Test(filterx_func_unset_empties, recursive)
{
  GList *args = NULL;

  /* dict */

  /* default is true */
  args = g_list_append(args, filterx_function_arg_new(NULL,
                                                      filterx_literal_new(filterx_json_new_from_repr("[{\"foo\": 0}]", -1))));
  _assert_unset_empties(args, "[]");
  args = NULL;

  args = g_list_append(args, filterx_function_arg_new(NULL,
                                                      filterx_literal_new(filterx_json_new_from_repr("[{\"foo\": 0}]", -1))));
  args = g_list_append(args, filterx_function_arg_new("recursive", filterx_literal_new(filterx_boolean_new(FALSE))));
  _assert_unset_empties(args, "[{\"foo\":0}]");
  args = NULL;

  /* list */

  /* default is true */
  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_json_new_from_repr("[[0]]",
                                                      -1))));
  _assert_unset_empties(args, "[]");
  args = NULL;

  args = g_list_append(args, filterx_function_arg_new(NULL, filterx_literal_new(filterx_json_new_from_repr("[[0]]",
                                                      -1))));
  args = g_list_append(args, filterx_function_arg_new("recursive", filterx_literal_new(filterx_boolean_new(FALSE))));
  _assert_unset_empties(args, "[[0]]");
  args = NULL;
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

TestSuite(filterx_func_unset_empties, .init = setup, .fini = teardown);
