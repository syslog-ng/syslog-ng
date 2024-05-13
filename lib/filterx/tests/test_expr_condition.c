/*
 * Copyright (c) 2023 shifter <shifter@axoflow.com>
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

#include "filterx/filterx-eval.h"
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


FilterXExpr *
_string_to_filterXExpr(const gchar *str)
{
  return filterx_literal_new(filterx_string_new(str, -1));
}

gint
_assert_cmp_string_to_filterx_object(const char *str, FilterXObject *obj)
{
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(string)));
  gsize string_len;
  const gchar *string = filterx_string_get_value(obj, &string_len);
  return strcmp(string, str);
}

FilterXExpr *
_assert_assign_var(const char *var_name, FilterXExpr *value)
{
  FilterXExpr *control_variable = filterx_msg_variable_expr_new(var_name);
  cr_assert(control_variable != NULL);

  return filterx_assign_new(control_variable, value);
}

void
_assert_set_test_variable(const char *var_name, FilterXExpr *expr)
{
  FilterXExpr *assign = _assert_assign_var(var_name, expr);
  cr_assert(assign != NULL);

  FilterXObject *assign_eval_res = filterx_expr_eval(assign);
  cr_assert(assign_eval_res != NULL);
  cr_assert(filterx_object_truthy(assign_eval_res));

  filterx_expr_unref(assign);
  filterx_object_unref(assign_eval_res);
}

FilterXObject *
_assert_get_test_variable(const char *var_name)
{
  FilterXExpr *control_variable = filterx_msg_variable_expr_new(var_name);
  cr_assert(control_variable != NULL);
  FilterXObject *result = filterx_expr_eval(control_variable);
  filterx_expr_unref(control_variable);
  return result;
}

//// Actual tests

Test(expr_condition, test_control_variable_set_get)
{
  FilterXObject *control_value = _assert_get_test_variable("$control-value");

  cr_assert_eq(0, _assert_cmp_string_to_filterx_object("default", control_value));

  filterx_object_unref(control_value);
}


Test(expr_condition, test_condition_with_null_expr_must_evaluated)
{
  GList *stmts = g_list_append(NULL, _assert_assign_var("$control-value", _string_to_filterXExpr("matching")));

  FilterXExpr *cond = filterx_conditional_new_codeblock(stmts);
  FilterXObject *cond_eval = filterx_expr_eval(cond);
  cr_assert(cond_eval != NULL);
  cr_assert(filterx_object_truthy(cond_eval) == TRUE);

  FilterXObject *control_value = _assert_get_test_variable("$control-value");
  cr_assert_eq(0, _assert_cmp_string_to_filterx_object("matching", control_value));

  filterx_expr_unref(cond);
  filterx_object_unref(control_value);
  filterx_object_unref(cond_eval);
}

Test(expr_condition, test_condition_matching_expression)
{
  GList *stmts = g_list_append(NULL, _assert_assign_var("$control-value", _string_to_filterXExpr("matching")));

  FilterXExpr *cond = filterx_conditional_new_conditional_codeblock(filterx_literal_new(filterx_boolean_new(true)),
                      stmts);
  FilterXObject *cond_eval = filterx_expr_eval(cond);
  cr_assert(cond_eval != NULL);
  cr_assert(filterx_object_truthy(cond_eval) == TRUE);

  FilterXObject *control_value = _assert_get_test_variable("$control-value");
  cr_assert_eq(0, _assert_cmp_string_to_filterx_object("matching", control_value));

  filterx_expr_unref(cond);
  filterx_object_unref(control_value);
  filterx_object_unref(cond_eval);
}


Test(expr_condition, test_condition_non_matching_expression)
{
  GList *stmts = g_list_append(NULL, _assert_assign_var("$control-value", _string_to_filterXExpr("matching")));

  FilterXExpr *cond = filterx_conditional_new_conditional_codeblock(filterx_literal_new(filterx_boolean_new(false)),
                      stmts);
  FilterXObject *cond_eval = filterx_expr_eval(cond);
  cr_assert(cond_eval != NULL);
  cr_assert(filterx_object_truthy(cond_eval) == TRUE);

  FilterXObject *control_value = _assert_get_test_variable("$control-value");
  cr_assert_eq(0, _assert_cmp_string_to_filterx_object("default", control_value));

  filterx_expr_unref(cond);
  filterx_object_unref(control_value);
  filterx_object_unref(cond_eval);
}


Test(expr_condition, test_condition_matching_elif_expression)
{
  GList *stmts = g_list_append(NULL, _assert_assign_var("$control-value", _string_to_filterXExpr("matching")));
  GList *elif_stmts = g_list_append(NULL, _assert_assign_var("$control-value", _string_to_filterXExpr("elif-matching")));

  FilterXExpr *cond = filterx_conditional_new_conditional_codeblock(filterx_literal_new(filterx_boolean_new(false)),
                      stmts);
  cond = filterx_conditional_add_false_branch((FilterXConditional *)cond,
                                              (FilterXConditional *)filterx_conditional_new_conditional_codeblock(filterx_literal_new(filterx_boolean_new(true)),
                                                  elif_stmts));

  FilterXObject *cond_eval = filterx_expr_eval(cond);
  cr_assert(cond_eval != NULL);
  cr_assert(filterx_object_truthy(cond_eval) == TRUE);

  FilterXObject *control_value = _assert_get_test_variable("$control-value");
  cr_assert_eq(0, _assert_cmp_string_to_filterx_object("elif-matching", control_value));

  filterx_expr_unref(cond);
  filterx_object_unref(control_value);
  filterx_object_unref(cond_eval);
}


Test(expr_condition, test_condition_non_matching_elif_expression)
{
  GList *stmts = g_list_append(NULL, _assert_assign_var("$control-value", _string_to_filterXExpr("matching")));
  GList *elif_stmts = g_list_append(NULL, _assert_assign_var("$control-value", _string_to_filterXExpr("elif-matching")));

  FilterXExpr *cond = filterx_conditional_new_conditional_codeblock(filterx_literal_new(filterx_boolean_new(false)),
                      stmts);
  cond = filterx_conditional_add_false_branch((FilterXConditional *)cond,
                                              (FilterXConditional *)filterx_conditional_new_conditional_codeblock(filterx_literal_new(filterx_boolean_new(false)),
                                                  elif_stmts));

  FilterXObject *cond_eval = filterx_expr_eval(cond);
  cr_assert(cond_eval != NULL);
  cr_assert(filterx_object_truthy(cond_eval) == TRUE);

  FilterXObject *control_value = _assert_get_test_variable("$control-value");
  cr_assert_eq(0, _assert_cmp_string_to_filterx_object("default", control_value));

  filterx_expr_unref(cond);
  filterx_object_unref(control_value);
  filterx_object_unref(cond_eval);
}

Test(expr_condition, test_condition_matching_else_expression)
{
  GList *stmts = g_list_append(NULL, _assert_assign_var("$control-value", _string_to_filterXExpr("matching")));
  GList *elif_stmts = g_list_append(NULL, _assert_assign_var("$control-value", _string_to_filterXExpr("elif-matching")));
  GList *else_stmts = g_list_append(NULL, _assert_assign_var("$control-value", _string_to_filterXExpr("else-matching")));

  FilterXExpr *cond = filterx_conditional_new_conditional_codeblock(filterx_literal_new(filterx_boolean_new(false)),
                      stmts);
  cond = filterx_conditional_add_false_branch((FilterXConditional *)cond,
                                              (FilterXConditional *)filterx_conditional_new_conditional_codeblock(filterx_literal_new(filterx_boolean_new(false)),
                                                  elif_stmts));
  cond = filterx_conditional_add_false_branch((FilterXConditional *)cond,
                                              (FilterXConditional *)filterx_conditional_new_conditional_codeblock(NULL, else_stmts));


  FilterXObject *cond_eval = filterx_expr_eval(cond);
  cr_assert(cond_eval != NULL);
  cr_assert(filterx_object_truthy(cond_eval) == TRUE);

  FilterXObject *control_value = _assert_get_test_variable("$control-value");
  cr_assert_eq(0, _assert_cmp_string_to_filterx_object("else-matching", control_value));

  filterx_expr_unref(cond);
  filterx_object_unref(control_value);
  filterx_object_unref(cond_eval);
}

Test(expr_condition, test_condition_subsequent_conditions_must_create_nested_condition)
{
  GList *stmts = g_list_append(NULL, _assert_assign_var("$control-value", _string_to_filterXExpr("matching")));
  GList *elif_stmts = g_list_append(NULL, _assert_assign_var("$control-value", _string_to_filterXExpr("elif-matching")));
  GList *elif2_stmts = g_list_append(NULL, _assert_assign_var("$control-value",
                                                              _string_to_filterXExpr("elif2-matching")));
  GList *else_stmts = g_list_append(NULL, _assert_assign_var("$control-value", _string_to_filterXExpr("else-matching")));

  FilterXExpr *cond = filterx_conditional_new_conditional_codeblock(filterx_literal_new(filterx_boolean_new(false)),
                      stmts);
  FilterXConditional *cond2 = (FilterXConditional *)filterx_conditional_new_conditional_codeblock(filterx_literal_new(
                                filterx_boolean_new(false)),
                              elif_stmts);
  FilterXConditional *cond3 = (FilterXConditional *)filterx_conditional_new_conditional_codeblock(filterx_literal_new(
                                filterx_boolean_new(true)),
                              elif2_stmts);
  FilterXConditional *cond4 = (FilterXConditional *)filterx_conditional_new_conditional_codeblock(NULL, else_stmts);
  cond = filterx_conditional_add_false_branch((FilterXConditional *)cond, cond2);
  cond = filterx_conditional_add_false_branch((FilterXConditional *)cond, cond3);
  cond = filterx_conditional_add_false_branch((FilterXConditional *)cond, cond4);

  cr_assert(((FilterXConditional *)cond)->false_branch == cond2);
  cr_assert(((FilterXConditional *)cond2)->false_branch == cond3);
  cr_assert(((FilterXConditional *)cond3)->false_branch == cond4);
  cr_assert(((FilterXConditional *)cond4)->false_branch == NULL);

  filterx_expr_unref(cond);
}

Test(expr_condition, test_condition_all_the_statements_must_executed)
{
  GList *stmts = g_list_append(NULL, _assert_assign_var("$control-value", _string_to_filterXExpr("matching")));
  stmts = g_list_append(stmts, _assert_assign_var("$control-value2", _string_to_filterXExpr("matching2")));
  stmts = g_list_append(stmts, _assert_assign_var("$control-value3", _string_to_filterXExpr("matching3")));

  FilterXExpr *cond = filterx_conditional_new_conditional_codeblock(filterx_literal_new(filterx_boolean_new(true)),
                      stmts);

  FilterXObject *cond_eval = filterx_expr_eval(cond);
  cr_assert(cond_eval != NULL);
  cr_assert(filterx_object_truthy(cond_eval) == TRUE);

  FilterXObject *control_value = _assert_get_test_variable("$control-value");
  cr_assert_eq(0, _assert_cmp_string_to_filterx_object("matching", control_value));
  filterx_object_unref(control_value);
  control_value = _assert_get_test_variable("$control-value2");
  cr_assert_eq(0, _assert_cmp_string_to_filterx_object("matching2", control_value));
  filterx_object_unref(control_value);
  control_value = _assert_get_test_variable("$control-value3");
  cr_assert_eq(0, _assert_cmp_string_to_filterx_object("matching3", control_value));
  filterx_object_unref(control_value);

  filterx_expr_unref(cond);
  filterx_object_unref(cond_eval);
}


Test(expr_condition, test_condition_must_return_last_expression_from_evaluated_codeblock)
{
  GList *stmts = g_list_append(NULL, _assert_assign_var("$control-value", _string_to_filterXExpr("matching")));
  stmts = g_list_append(stmts, _assert_assign_var("$control-value2", _string_to_filterXExpr("matching2")));
  stmts = g_list_append(stmts, _string_to_filterXExpr("must be returned"));

  FilterXExpr *cond = filterx_conditional_new_conditional_codeblock(filterx_literal_new(filterx_boolean_new(true)),
                      stmts);

  FilterXObject *cond_eval = filterx_expr_eval(cond);
  cr_assert(cond_eval != NULL);
  _assert_cmp_string_to_filterx_object("must be returned", cond_eval);

  filterx_expr_unref(cond);
  filterx_object_unref(cond_eval);
}


Test(expr_condition, test_condition_falsey_expression_must_interrupt_sequential_code_execution)
{
  GList *stmts = g_list_append(NULL, _assert_assign_var("$control-value", _string_to_filterXExpr("matching")));
  stmts = g_list_append(stmts, filterx_literal_new(filterx_boolean_new(false)));
  stmts = g_list_append(stmts, _assert_assign_var("$control-value3", _string_to_filterXExpr("matching3")));

  FilterXExpr *cond = filterx_conditional_new_conditional_codeblock(filterx_literal_new(filterx_boolean_new(true)),
                      stmts);

  FilterXObject *cond_eval = filterx_expr_eval(cond);
  cr_assert(cond_eval != NULL);
  cr_assert(filterx_object_truthy(cond_eval) == FALSE);

  FilterXObject *control_value = _assert_get_test_variable("$control-value");
  cr_assert_eq(0, _assert_cmp_string_to_filterx_object("matching", control_value));
  filterx_object_unref(control_value);
  control_value = _assert_get_test_variable("$control-value3");
  cr_assert_eq(0, _assert_cmp_string_to_filterx_object("default3", control_value));
  filterx_object_unref(control_value);

  filterx_expr_unref(cond);
  filterx_object_unref(cond_eval);
}


Test(expr_condition, test_condition_do_not_allow_to_add_else_into_else, .signal=SIGABRT)
{
  GList *stmts = g_list_append(NULL, _assert_assign_var("$control-value", _string_to_filterXExpr("matching")));
  GList *stmts2 = g_list_append(NULL, _assert_assign_var("$control-value", _string_to_filterXExpr("matching")));

  FilterXExpr *cond = filterx_conditional_new_codeblock(stmts);
  cond = filterx_conditional_add_false_branch((FilterXConditional *) cond,
                                              (FilterXConditional *) filterx_conditional_new_codeblock(stmts2));

  filterx_expr_unref(cond);
}

FilterXObject *
_fail_func(GPtrArray *args)
{
  return NULL;
}

Test(expr_condition, test_condition_return_null_on_illegal_expr)
{
  GList *stmts = g_list_append(NULL, _assert_assign_var("$control-value", _string_to_filterXExpr("matching")));

  FilterXExpr *func = filterx_simple_function_new("test_fn", filterx_function_args_new(NULL, NULL), _fail_func);

  FilterXExpr *cond = filterx_conditional_new_conditional_codeblock(func, stmts);
  FilterXObject *res = filterx_expr_eval(cond);
  cr_assert_null(res);

  filterx_expr_unref(cond);
}

FilterXObject *
_dummy_func(GPtrArray *args)
{
  return filterx_string_new("foobar", -1);
}

Test(expr_condition, test_condition_return_expr_result_on_missing_stmts)
{
  FilterXExpr *func = filterx_simple_function_new("test_fn", filterx_function_args_new(NULL, NULL), _dummy_func);

  FilterXExpr *cond = filterx_conditional_new_conditional_codeblock(func, NULL);
  FilterXObject *res = filterx_expr_eval(cond);
  cr_assert_not_null(res);
  cr_assert(filterx_object_is_type(res, &FILTERX_TYPE_NAME(string)));
  const gchar *strval = filterx_string_get_value(res, NULL);
  cr_assert_str_eq(strval, "foobar");

  filterx_expr_unref(cond);
  filterx_object_unref(res);
}

Test(expr_condition, test_condition_must_not_fail_on_empty_else_block)
{
  FilterXExpr *cond = filterx_conditional_new_conditional_codeblock(filterx_literal_new(filterx_boolean_new(FALSE)),
                      NULL);
  cond = filterx_conditional_add_false_branch((FilterXConditional *)cond,
                                              (FilterXConditional *)filterx_conditional_new_codeblock(NULL));
  FilterXObject *res = filterx_expr_eval(cond);
  cr_assert_not_null(res);
  cr_assert(filterx_object_is_type(res, &FILTERX_TYPE_NAME(boolean)));
  cr_assert(filterx_object_truthy(res));

  filterx_expr_unref(cond);
  filterx_object_unref(res);
}

Test(expr_condition, test_condition_with_complex_expression_to_check_memory_leaks)
{
  GList *stmts = NULL;
  stmts = g_list_append(stmts, filterx_literal_new(filterx_string_new("foobar", -1)));

  FilterXExpr *cond = filterx_conditional_new_conditional_codeblock(filterx_literal_new(filterx_integer_new(0)), NULL);
  cond = filterx_conditional_add_false_branch((FilterXConditional *)cond,
                                              (FilterXConditional *)filterx_conditional_new_codeblock(stmts));
  FilterXObject *res = filterx_expr_eval(cond);
  cr_assert_not_null(res);
  cr_assert(filterx_object_is_type(res, &FILTERX_TYPE_NAME(string)));
  const gchar *str = filterx_string_get_value(res, NULL);
  cr_assert_str_eq(str, "foobar");

  filterx_expr_unref(cond);
  filterx_object_unref(res);
}

static void
setup(void)
{
  app_startup();
  init_libtest_filterx();
  _assert_set_test_variable("$control-value", _string_to_filterXExpr("default"));
  _assert_set_test_variable("$control-value3", _string_to_filterXExpr("default3"));
}

static void
teardown(void)
{
  scratch_buffers_explicit_gc();
  deinit_libtest_filterx();
  app_shutdown();
}

TestSuite(expr_condition, .init = setup, .fini = teardown);
