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

#include <criterion/criterion.h>
#include "libtest/cr_template.h"

#include "filterx/expr-literal.h"
#include "filterx/expr-template.h"
#include "filterx/expr-list.h"
#include "filterx/expr-dict.h"
#include "filterx/expr-merge.h"
#include "filterx/object-primitive.h"
#include "filterx/object-string.h"
#include "filterx/object-json.h"
#include "filterx/object-list-interface.h"
#include "filterx/object-dict-interface.h"

#include "apphook.h"
#include "scratch-buffers.h"

#include "filterx-lib.h"

static void
_assert_int_value_and_unref(FilterXObject *object, gint64 expected_value)
{
  gint64 value;
  cr_assert(filterx_integer_unwrap(object, &value));
  cr_assert_eq(value, expected_value);
  filterx_object_unref(object);
}

Test(filterx_expr, test_filterx_expr_construction_and_free)
{
  FilterXExpr *fexpr = filterx_expr_new();

  filterx_expr_unref(fexpr);
}

Test(filterx_expr, test_filterx_literal_evaluates_to_the_literal_object)
{
  FilterXExpr *fexpr = filterx_literal_new(filterx_integer_new(42));
  FilterXObject *fobj = filterx_expr_eval(fexpr);

  assert_marshaled_object(fobj, "42", LM_VT_INTEGER);

  filterx_expr_unref(fexpr);
  filterx_object_unref(fobj);
}

Test(filterx_expr, test_filterx_template_evaluates_to_the_expanded_value)
{
  LogMessage *msg = create_sample_message();
  FilterXScope *scope = filterx_scope_new();

  FilterXEvalContext context =
  {
    .msgs = &msg,
    .num_msg = 1,
    .template_eval_options = &DEFAULT_TEMPLATE_EVAL_OPTIONS,
    .scope = scope,
  };

  filterx_eval_set_context(&context);
  FilterXExpr *fexpr = filterx_template_new(compile_template("$HOST $PROGRAM"));
  FilterXObject *fobj = filterx_expr_eval(fexpr);

  assert_marshaled_object(fobj, "bzorp syslog-ng", LM_VT_STRING);

  filterx_expr_unref(fexpr);
  log_msg_unref(msg);
  filterx_object_unref(fobj);
  filterx_scope_free(scope);
  filterx_eval_set_context(NULL);
}

struct _FilterXScope
{
  GHashTable *value_cache;
  GPtrArray *weak_refs;
};

Test(filterx_expr, test_filterx_list_merge)
{
  LogMessage *msg = create_sample_message();
  FilterXScope *scope = filterx_scope_new();

  FilterXEvalContext context =
  {
    .msgs = &msg,
    .num_msg = 1,
    .template_eval_options = &DEFAULT_TEMPLATE_EVAL_OPTIONS,
    .scope = scope,
  };
  filterx_eval_set_context(&context);

  // $fillable = json_array();
  FilterXObject *json_array = filterx_json_array_new_empty();
  FilterXExpr *fillable = filterx_literal_new(json_array);
  FilterXExpr *list_expr = NULL;
  FilterXObject *result = NULL;
  GList *values = NULL, *inner_values = NULL;


  // [42];
  values = g_list_append(NULL, filterx_literal_new(filterx_integer_new(42)));
  list_expr = filterx_list_expr_new(fillable, values);

  // $fillable << [42];
  result = filterx_expr_eval(list_expr);
  cr_assert_eq(result, json_array);
  cr_assert(filterx_object_is_type(result, &FILTERX_TYPE_NAME(list)));
  cr_assert_eq(filterx_list_len(result), 1);
  _assert_int_value_and_unref(filterx_list_get_subscript(result, 0), 42);
  filterx_object_unref(result);

  // $fillable << [42];
  result = filterx_expr_eval(list_expr);
  cr_assert_eq(result, json_array);
  cr_assert_eq(filterx_list_len(result), 2);
  _assert_int_value_and_unref(filterx_list_get_subscript(result, 0), 42);
  _assert_int_value_and_unref(filterx_list_get_subscript(result, 1), 42);
  filterx_object_unref(result);

  filterx_expr_unref(list_expr);


  // [[1337]];
  inner_values = g_list_append(NULL, filterx_literal_new(filterx_integer_new(1337)));
  values = g_list_append(NULL, filterx_list_expr_inner_new(fillable, inner_values));
  list_expr = filterx_list_expr_new(fillable, values);

  // $fillable << [[1337]];
  result = filterx_expr_eval(list_expr);
  cr_assert_eq(result, json_array);
  cr_assert_eq(filterx_list_len(result), 3);

  FilterXObject *stored_inner_list = filterx_list_get_subscript(result, 2);
  cr_assert(stored_inner_list);
  cr_assert(filterx_object_is_type(stored_inner_list, &FILTERX_TYPE_NAME(list)));
  cr_assert_eq(filterx_list_len(stored_inner_list), 1);
  _assert_int_value_and_unref(filterx_list_get_subscript(stored_inner_list, 0), 1337);
  filterx_object_unref(stored_inner_list);

  filterx_object_unref(result);
  filterx_expr_unref(list_expr);


  filterx_expr_unref(fillable);
  log_msg_unref(msg);
  filterx_scope_free(scope);
  filterx_eval_set_context(NULL);
}

Test(filterx_expr, test_filterx_dict_merge)
{
  LogMessage *msg = create_sample_message();
  FilterXScope *scope = filterx_scope_new();

  FilterXEvalContext context =
  {
    .msgs = &msg,
    .num_msg = 1,
    .template_eval_options = &DEFAULT_TEMPLATE_EVAL_OPTIONS,
    .scope = scope,
  };
  filterx_eval_set_context(&context);

  // $fillable = json();
  FilterXObject *json = filterx_json_object_new_empty();
  FilterXExpr *fillable = filterx_literal_new(json);
  FilterXExpr *dict_expr = NULL;
  FilterXObject *result = NULL;
  GList *values = NULL, *inner_values = NULL;

  FilterXObject *foo = filterx_string_new("foo", -1);
  FilterXObject *bar = filterx_string_new("bar", -1);
  FilterXObject *baz = filterx_string_new("baz", -1);


  // {"foo": 42};
  values = g_list_append(NULL, filterx_kv_new("foo", filterx_literal_new(filterx_integer_new(42))));
  dict_expr = filterx_dict_expr_new(fillable, values);

  // $fillable << {"foo": 42};
  result = filterx_expr_eval(dict_expr);
  cr_assert_eq(result, json);
  cr_assert(filterx_object_is_type(result, &FILTERX_TYPE_NAME(dict)));
  cr_assert_eq(filterx_dict_len(result), 1);
  _assert_int_value_and_unref(filterx_object_get_subscript(result, foo), 42);
  filterx_object_unref(result);

  // $fillable << {"foo": 42};
  result = filterx_expr_eval(dict_expr);
  cr_assert_eq(result, json);
  cr_assert_eq(filterx_dict_len(result), 1);
  _assert_int_value_and_unref(filterx_object_get_subscript(result, foo), 42);
  filterx_object_unref(result);

  filterx_expr_unref(dict_expr);


  // {"foo": 420};
  values = g_list_append(NULL, filterx_kv_new("foo", filterx_literal_new(filterx_integer_new(420))));
  dict_expr = filterx_dict_expr_new(fillable, values);

  // $fillable << {"foo": 420};
  result = filterx_expr_eval(dict_expr);
  cr_assert_eq(result, json);
  cr_assert(filterx_object_is_type(result, &FILTERX_TYPE_NAME(dict)));
  cr_assert_eq(filterx_dict_len(result), 1);
  _assert_int_value_and_unref(filterx_object_get_subscript(result, foo), 420);
  filterx_object_unref(result);

  filterx_expr_unref(dict_expr);


  // {"bar": 1337};
  values = g_list_append(NULL, filterx_kv_new("bar", filterx_literal_new(filterx_integer_new(1337))));
  dict_expr = filterx_dict_expr_new(fillable, values);

  // $fillable << {"bar": 1337};
  result = filterx_expr_eval(dict_expr);
  cr_assert_eq(result, json);
  cr_assert(filterx_object_is_type(result, &FILTERX_TYPE_NAME(dict)));
  cr_assert_eq(filterx_dict_len(result), 2);
  _assert_int_value_and_unref(filterx_object_get_subscript(result, foo), 420);
  _assert_int_value_and_unref(filterx_object_get_subscript(result, bar), 1337);
  filterx_object_unref(result);

  filterx_expr_unref(dict_expr);


  // {"baz": {"foo": 1}};
  inner_values = g_list_append(NULL, filterx_kv_new("foo", filterx_literal_new(filterx_integer_new(1))));
  values = g_list_append(NULL, filterx_kv_new("baz", filterx_dict_expr_inner_new(fillable, inner_values)));
  dict_expr = filterx_dict_expr_new(fillable, values);

  // $fillable << {"baz": {"foo": 1}};
  result = filterx_expr_eval(dict_expr);
  cr_assert_eq(result, json);
  cr_assert(filterx_object_is_type(result, &FILTERX_TYPE_NAME(dict)));
  cr_assert_eq(filterx_dict_len(result), 3);

  FilterXObject *stored_inner_dict = filterx_object_get_subscript(result, baz);
  cr_assert(stored_inner_dict);
  cr_assert(filterx_object_is_type(stored_inner_dict, &FILTERX_TYPE_NAME(dict)));
  cr_assert_eq(filterx_dict_len(stored_inner_dict), 1);
  _assert_int_value_and_unref(filterx_object_get_subscript(stored_inner_dict, foo), 1);
  filterx_object_unref(stored_inner_dict);

  filterx_object_unref(result);

  // $fillable << {"foo": 1}};
  // Shallow merge.
  result = filterx_expr_eval(dict_expr);
  cr_assert_eq(result, json);
  cr_assert(filterx_object_is_type(result, &FILTERX_TYPE_NAME(dict)));
  cr_assert_eq(filterx_dict_len(result), 3);

  stored_inner_dict = filterx_object_get_subscript(result, baz);
  cr_assert(stored_inner_dict);
  cr_assert(filterx_object_is_type(stored_inner_dict, &FILTERX_TYPE_NAME(dict)));
  cr_assert_eq(filterx_dict_len(stored_inner_dict), 1);
  _assert_int_value_and_unref(filterx_object_get_subscript(stored_inner_dict, foo), 1);
  filterx_object_unref(stored_inner_dict);

  filterx_object_unref(result);
  filterx_expr_unref(dict_expr);


  filterx_object_unref(baz);
  filterx_object_unref(bar);
  filterx_object_unref(foo);
  filterx_expr_unref(fillable);
  log_msg_unref(msg);
  filterx_scope_free(scope);
  filterx_eval_set_context(NULL);
}

Test(filterx_expr, test_filterx_merge_expr_with_lists)
{
  LogMessage *msg = create_sample_message();
  FilterXScope *scope = filterx_scope_new();

  FilterXEvalContext context =
  {
    .msgs = &msg,
    .num_msg = 1,
    .template_eval_options = &DEFAULT_TEMPLATE_EVAL_OPTIONS,
    .scope = scope,
  };
  filterx_eval_set_context(&context);

  FilterXObject *fx_1 = filterx_integer_new(1);
  FilterXObject *fx_2 = filterx_integer_new(2);
  FilterXObject *fx_3 = filterx_integer_new(3);
  FilterXObject *fx_4 = filterx_integer_new(4);

  FilterXObject *lhs_obj = filterx_json_array_new_empty();
  FilterXObject *rhs_obj = filterx_json_array_new_empty();
  filterx_list_append(lhs_obj, fx_1);
  filterx_list_append(lhs_obj, fx_2);
  filterx_list_append(rhs_obj, fx_3);
  filterx_list_append(rhs_obj, fx_4);

  FilterXExpr *lhs = filterx_literal_new(lhs_obj);
  FilterXExpr *rhs = filterx_literal_new(rhs_obj);

  FilterXExpr *merge = filterx_merge_new(lhs, rhs);
  FilterXObject *result = filterx_expr_eval(merge);
  cr_assert(result);

  cr_assert(filterx_object_is_type(result, &FILTERX_TYPE_NAME(list)));
  cr_assert_eq(filterx_list_len(result), 4);
  _assert_int_value_and_unref(filterx_list_get_subscript(result, 0), 1);
  _assert_int_value_and_unref(filterx_list_get_subscript(result, 1), 2);
  _assert_int_value_and_unref(filterx_list_get_subscript(result, 2), 3);
  _assert_int_value_and_unref(filterx_list_get_subscript(result, 3), 4);

  filterx_object_unref(result);

  filterx_expr_unref(merge);
  filterx_object_unref(fx_1);
  filterx_object_unref(fx_2);
  filterx_object_unref(fx_3);
  filterx_object_unref(fx_4);
  log_msg_unref(msg);
  filterx_scope_free(scope);
  filterx_eval_set_context(NULL);
}

Test(filterx_expr, test_filterx_merge_expr_with_dicts)
{
  LogMessage *msg = create_sample_message();
  FilterXScope *scope = filterx_scope_new();

  FilterXEvalContext context =
  {
    .msgs = &msg,
    .num_msg = 1,
    .template_eval_options = &DEFAULT_TEMPLATE_EVAL_OPTIONS,
    .scope = scope,
  };
  filterx_eval_set_context(&context);

  FilterXObject *fx_foo = filterx_string_new("foo", -1);
  FilterXObject *fx_bar = filterx_string_new("bar", -1);
  FilterXObject *fx_baz = filterx_string_new("baz", -1);
  FilterXObject *fx_alm = filterx_string_new("almafa", -1);
  FilterXObject *fx_1 = filterx_integer_new(1);
  FilterXObject *fx_2 = filterx_integer_new(2);
  FilterXObject *fx_3 = filterx_integer_new(3);
  FilterXObject *fx_4 = filterx_integer_new(4);

  FilterXObject *lhs_obj = filterx_json_object_new_empty();
  FilterXObject *rhs_obj = filterx_json_object_new_empty();
  filterx_object_set_subscript(lhs_obj, fx_foo, fx_1);
  filterx_object_set_subscript(lhs_obj, fx_bar, fx_2);
  filterx_object_set_subscript(rhs_obj, fx_baz, fx_3);
  filterx_object_set_subscript(rhs_obj, fx_alm, fx_4);

  FilterXExpr *lhs = filterx_literal_new(lhs_obj);
  FilterXExpr *rhs = filterx_literal_new(rhs_obj);

  FilterXExpr *merge = filterx_merge_new(lhs, rhs);
  FilterXObject *result = filterx_expr_eval(merge);
  cr_assert(result);

  cr_assert(filterx_object_is_type(result, &FILTERX_TYPE_NAME(dict)));
  cr_assert_eq(filterx_dict_len(result), 4);
  _assert_int_value_and_unref(filterx_object_get_subscript(result, fx_foo), 1);
  _assert_int_value_and_unref(filterx_object_get_subscript(result, fx_bar), 2);
  _assert_int_value_and_unref(filterx_object_get_subscript(result, fx_baz), 3);
  _assert_int_value_and_unref(filterx_object_get_subscript(result, fx_alm), 4);

  filterx_object_unref(result);

  filterx_expr_unref(merge);
  filterx_object_unref(fx_1);
  filterx_object_unref(fx_2);
  filterx_object_unref(fx_3);
  filterx_object_unref(fx_4);
  filterx_object_unref(fx_foo);
  filterx_object_unref(fx_bar);
  filterx_object_unref(fx_baz);
  filterx_object_unref(fx_alm);
  log_msg_unref(msg);
  filterx_scope_free(scope);
  filterx_eval_set_context(NULL);
}

static void
setup(void)
{
  app_startup();
  init_template_tests();
}

static void
teardown(void)
{
  scratch_buffers_explicit_gc();
  deinit_template_tests();
  app_shutdown();
}

TestSuite(filterx_expr, .init = setup, .fini = teardown);
