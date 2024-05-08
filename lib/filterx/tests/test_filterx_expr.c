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

#include "filterx/filterx-scope.h"
#include "filterx/filterx-eval.h"
#include "filterx/expr-literal.h"
#include "filterx/expr-template.h"
#include "filterx/expr-literal-generator.h"
#include "filterx/object-primitive.h"
#include "filterx/object-string.h"
#include "filterx/object-json.h"
#include "filterx/object-list-interface.h"
#include "filterx/object-dict-interface.h"
#include "filterx/expr-assign.h"
#include "filterx/expr-variable.h"
#include "filterx/expr-setattr.h"
#include "filterx/expr-getattr.h"
#include "filterx/expr-set-subscript.h"
#include "filterx/expr-get-subscript.h"

#include "apphook.h"
#include "scratch-buffers.h"

#include "libtest/filterx-lib.h"

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
  FilterXExpr *fexpr = filterx_template_new(compile_template("$HOST $PROGRAM"));
  FilterXObject *fobj = filterx_expr_eval(fexpr);

  assert_marshaled_object(fobj, "bzorp syslog-ng", LM_VT_STRING);

  filterx_expr_unref(fexpr);
  filterx_object_unref(fobj);
}

struct _FilterXScope
{
  GHashTable *value_cache;
  GPtrArray *weak_refs;
};

Test(filterx_expr, test_filterx_list_merge)
{
  // $fillable = json_array();
  FilterXObject *json_array = filterx_json_array_new_empty();
  FilterXExpr *fillable = filterx_literal_new(json_array);
  FilterXExpr *list_expr = NULL;
  FilterXObject *result = NULL;
  GList *values = NULL, *inner_values = NULL;
  guint64 len;


  // [42];
  values = g_list_append(NULL, filterx_literal_generator_elem_new(NULL,
                         filterx_literal_new(filterx_integer_new(42)),
                         TRUE));
  list_expr = filterx_literal_list_generator_new();
  filterx_generator_set_fillable(list_expr, filterx_expr_ref(fillable));
  filterx_literal_generator_set_elements(list_expr, values);

  // $fillable += [42];
  result = filterx_expr_eval(list_expr);
  cr_assert(result);
  cr_assert(filterx_object_truthy(result));
  cr_assert(filterx_object_len(json_array, &len));
  cr_assert_eq(len, 1);
  _assert_int_value_and_unref(filterx_list_get_subscript(json_array, 0), 42);
  filterx_object_unref(result);

  // $fillable += [42];
  result = filterx_expr_eval(list_expr);
  cr_assert(result);
  cr_assert(filterx_object_truthy(result));
  cr_assert(filterx_object_len(json_array, &len));
  cr_assert_eq(len, 2);
  _assert_int_value_and_unref(filterx_list_get_subscript(json_array, 0), 42);
  _assert_int_value_and_unref(filterx_list_get_subscript(json_array, 1), 42);
  filterx_object_unref(result);

  filterx_expr_unref(list_expr);


  // [[1337]];
  list_expr = filterx_literal_list_generator_new();
  filterx_generator_set_fillable(list_expr, filterx_expr_ref(fillable));
  inner_values = g_list_append(NULL, filterx_literal_generator_elem_new(NULL,
                               filterx_literal_new(filterx_integer_new(1337)),
                               TRUE));
  values = g_list_append(NULL, filterx_literal_generator_elem_new(NULL,
                         filterx_literal_inner_list_generator_new(list_expr, inner_values),
                         FALSE));
  filterx_literal_generator_set_elements(list_expr, values);

  // $fillable += [[1337]];
  result = filterx_expr_eval(list_expr);
  cr_assert(result);
  cr_assert(filterx_object_truthy(result));
  cr_assert(filterx_object_len(json_array, &len));
  cr_assert_eq(len, 3);

  FilterXObject *stored_inner_list = filterx_list_get_subscript(json_array, 2);
  cr_assert(stored_inner_list);
  cr_assert(filterx_object_is_type(stored_inner_list, &FILTERX_TYPE_NAME(list)));
  cr_assert(filterx_object_len(stored_inner_list, &len));
  cr_assert_eq(len, 1);
  _assert_int_value_and_unref(filterx_list_get_subscript(stored_inner_list, 0), 1337);
  filterx_object_unref(stored_inner_list);

  filterx_object_unref(result);
  filterx_expr_unref(list_expr);


  filterx_expr_unref(fillable);
}

Test(filterx_expr, test_filterx_dict_merge)
{
  // $fillable = json();
  FilterXObject *json = filterx_json_object_new_empty();
  FilterXExpr *fillable = filterx_literal_new(json);
  FilterXExpr *dict_expr = NULL;
  FilterXObject *result = NULL;
  GList *values = NULL, *inner_values = NULL;
  guint64 len;

  FilterXObject *foo = filterx_string_new("foo", -1);
  FilterXObject *bar = filterx_string_new("bar", -1);
  FilterXObject *baz = filterx_string_new("baz", -1);


  // {"foo": 42};
  values = g_list_append(NULL, filterx_literal_generator_elem_new(filterx_literal_new(filterx_object_ref(foo)),
                         filterx_literal_new(filterx_integer_new(42)),
                         TRUE));
  dict_expr = filterx_literal_dict_generator_new();
  filterx_generator_set_fillable(dict_expr, filterx_expr_ref(fillable));
  filterx_literal_generator_set_elements(dict_expr, values);

  // $fillable += {"foo": 42};
  result = filterx_expr_eval(dict_expr);
  cr_assert(result);
  cr_assert(filterx_object_truthy(result));
  cr_assert(filterx_object_len(json, &len));
  cr_assert_eq(len, 1);
  _assert_int_value_and_unref(filterx_object_get_subscript(json, foo), 42);
  filterx_object_unref(result);

  // $fillable += {"foo": 42};
  result = filterx_expr_eval(dict_expr);
  cr_assert(result);
  cr_assert(filterx_object_truthy(result));
  cr_assert(filterx_object_len(json, &len));
  cr_assert_eq(len, 1);
  _assert_int_value_and_unref(filterx_object_get_subscript(json, foo), 42);
  filterx_object_unref(result);

  filterx_expr_unref(dict_expr);


  // {"foo": 420};
  values = g_list_append(NULL, filterx_literal_generator_elem_new(filterx_literal_new(filterx_object_ref(foo)),
                         filterx_literal_new(filterx_integer_new(420)),
                         TRUE));
  dict_expr = filterx_literal_dict_generator_new();
  filterx_generator_set_fillable(dict_expr, filterx_expr_ref(fillable));
  filterx_literal_generator_set_elements(dict_expr, values);

  // $fillable += {"foo": 420};
  result = filterx_expr_eval(dict_expr);
  cr_assert(result);
  cr_assert(filterx_object_truthy(result));
  cr_assert(filterx_object_len(json, &len));
  cr_assert_eq(len, 1);
  _assert_int_value_and_unref(filterx_object_get_subscript(json, foo), 420);
  filterx_object_unref(result);

  filterx_expr_unref(dict_expr);


  // {"bar": 1337};
  values = g_list_append(NULL, filterx_literal_generator_elem_new(filterx_literal_new(filterx_object_ref(bar)),
                         filterx_literal_new(filterx_integer_new(1337)),
                         TRUE));
  dict_expr = filterx_literal_dict_generator_new();
  filterx_generator_set_fillable(dict_expr, filterx_expr_ref(fillable));
  filterx_literal_generator_set_elements(dict_expr, values);

  // $fillable += {"bar": 1337};
  result = filterx_expr_eval(dict_expr);
  cr_assert(result);
  cr_assert(filterx_object_truthy(result));
  cr_assert(filterx_object_len(json, &len));
  cr_assert_eq(len, 2);
  _assert_int_value_and_unref(filterx_object_get_subscript(json, foo), 420);
  _assert_int_value_and_unref(filterx_object_get_subscript(json, bar), 1337);
  filterx_object_unref(result);

  filterx_expr_unref(dict_expr);


  // {"baz": {"foo": 1}};
  dict_expr = filterx_literal_dict_generator_new();
  filterx_generator_set_fillable(dict_expr, filterx_expr_ref(fillable));
  inner_values = g_list_append(NULL, filterx_literal_generator_elem_new(filterx_literal_new(filterx_object_ref(foo)),
                               filterx_literal_new(filterx_integer_new(1)),
                               TRUE));
  values = g_list_append(NULL, filterx_literal_generator_elem_new(filterx_literal_new(filterx_object_ref(baz)),
                         filterx_literal_inner_dict_generator_new(dict_expr, inner_values),
                         FALSE));
  filterx_literal_generator_set_elements(dict_expr, values);

  // $fillable += {"baz": {"foo": 1}};
  result = filterx_expr_eval(dict_expr);
  cr_assert(result);
  cr_assert(filterx_object_truthy(result));
  cr_assert(filterx_object_len(json, &len));
  cr_assert_eq(len, 3);

  FilterXObject *stored_inner_dict = filterx_object_get_subscript(json, baz);
  cr_assert(stored_inner_dict);
  cr_assert(filterx_object_is_type(stored_inner_dict, &FILTERX_TYPE_NAME(dict)));
  cr_assert_eq(filterx_object_len(stored_inner_dict, &len), 1);
  _assert_int_value_and_unref(filterx_object_get_subscript(stored_inner_dict, foo), 1);
  filterx_object_unref(stored_inner_dict);

  filterx_object_unref(result);

  // $fillable += {"foo": 1}};
  // Shallow merge.
  result = filterx_expr_eval(dict_expr);
  cr_assert(result);
  cr_assert(filterx_object_truthy(result));
  cr_assert(filterx_object_len(json, &len));
  cr_assert_eq(len, 3);

  stored_inner_dict = filterx_object_get_subscript(json, baz);
  cr_assert(stored_inner_dict);
  cr_assert(filterx_object_is_type(stored_inner_dict, &FILTERX_TYPE_NAME(dict)));
  cr_assert(filterx_object_len(stored_inner_dict, &len));
  cr_assert_eq(len, 1);
  _assert_int_value_and_unref(filterx_object_get_subscript(stored_inner_dict, foo), 1);
  filterx_object_unref(stored_inner_dict);

  filterx_object_unref(result);
  filterx_expr_unref(dict_expr);


  filterx_object_unref(baz);
  filterx_object_unref(bar);
  filterx_object_unref(foo);
  filterx_expr_unref(fillable);
}

Test(filterx_expr, test_filterx_assign)
{
  FilterXExpr *result_var = filterx_msg_variable_expr_new("$result-var");
  cr_assert(result_var != NULL);

  FilterXExpr *assign = filterx_assign_new(result_var, filterx_literal_new(filterx_string_new("foobar", -1)));

  FilterXObject *res = filterx_expr_eval(assign);
  cr_assert_not_null(res);
  cr_assert(filterx_object_is_type(res, &FILTERX_TYPE_NAME(string)));
  cr_assert_str_eq(filterx_string_get_value(res, NULL), "foobar");
  cr_assert(filterx_object_truthy(res));
  cr_assert(assign->ignore_falsy_result);

  cr_assert_not_null(result_var);
  FilterXObject *result_obj = filterx_expr_eval(result_var);
  cr_assert_not_null(result_obj);
  cr_assert(filterx_object_is_type(result_obj, &FILTERX_TYPE_NAME(string)));
  const gchar *result_val = filterx_string_get_value(result_obj, NULL);
  cr_assert_str_eq("foobar", result_val);

  filterx_object_unref(res);
  filterx_expr_unref(assign);
  filterx_object_unref(result_obj);
}

Test(filterx_expr, test_filterx_setattr)
{
  FilterXObject *json = filterx_json_object_new_empty();
  FilterXExpr *fillable = filterx_literal_new(json);

  FilterXExpr *setattr = filterx_setattr_new(fillable, "foo", filterx_literal_new(filterx_string_new("bar", -1)));
  cr_assert_not_null(setattr);

  FilterXObject *res = filterx_expr_eval(setattr);
  cr_assert_not_null(res);
  cr_assert(filterx_object_is_type(res, &FILTERX_TYPE_NAME(string)));
  cr_assert_str_eq(filterx_string_get_value(res, NULL), "bar");
  cr_assert(filterx_object_truthy(res));
  cr_assert(setattr->ignore_falsy_result);

  assert_object_json_equals(json, "{\"foo\":\"bar\"}");

  filterx_expr_unref(setattr);
}

Test(filterx_expr, test_filterx_set_subscript)
{
  FilterXObject *json = filterx_json_object_new_empty();
  FilterXExpr *fillable = filterx_literal_new(json);

  FilterXExpr *setattr = filterx_set_subscript_new(fillable,
                                                   filterx_literal_new(filterx_string_new("foo", -1)),
                                                   filterx_literal_new(filterx_string_new("bar", -1)));
  cr_assert_not_null(setattr);

  FilterXObject *res = filterx_expr_eval(setattr);
  cr_assert_not_null(res);
  cr_assert(filterx_object_is_type(res, &FILTERX_TYPE_NAME(string)));
  cr_assert_str_eq(filterx_string_get_value(res, NULL), "bar");
  cr_assert(filterx_object_truthy(res));
  cr_assert(setattr->ignore_falsy_result);

  assert_object_json_equals(json, "{\"foo\":\"bar\"}");

  filterx_expr_unref(setattr);
}

Test(filterx_expr, test_filterx_readonly)
{
  FilterXObject *foo = filterx_string_new("foo", -1);
  FilterXObject *bar = filterx_string_new("bar", -1);

  FilterXObject *dict = filterx_test_dict_new();

  FilterXObject *inner_dict = filterx_test_dict_new();
  cr_assert(filterx_object_set_subscript(dict, foo, &inner_dict));
  filterx_object_unref(inner_dict);

  filterx_object_make_readonly(dict);

  FilterXExpr *literal = filterx_literal_new(dict);


  FilterXExpr *setattr = filterx_setattr_new(filterx_expr_ref(literal),
                                             "bar",
                                             filterx_literal_new(filterx_object_ref(foo)));
  cr_assert_not(filterx_expr_eval(setattr));
  cr_assert(strstr(filterx_eval_get_last_error(), "readonly"));
  filterx_eval_clear_errors();
  filterx_expr_unref(setattr);


  FilterXExpr *set_subscript = filterx_set_subscript_new(filterx_expr_ref(literal),
                                                         filterx_literal_new(filterx_object_ref(bar)),
                                                         filterx_literal_new(filterx_object_ref(foo)));
  cr_assert_not(filterx_expr_eval(set_subscript));
  cr_assert(strstr(filterx_eval_get_last_error(), "readonly"));
  filterx_eval_clear_errors();
  filterx_expr_unref(set_subscript);


  FilterXExpr *getattr = filterx_getattr_new(filterx_expr_ref(literal), "foo");
  cr_assert_not(filterx_expr_unset(getattr));
  cr_assert(strstr(filterx_eval_get_last_error(), "readonly"));
  filterx_eval_clear_errors();
  filterx_expr_unref(getattr);


  FilterXExpr *get_subscript = filterx_get_subscript_new(filterx_expr_ref(literal),
                                                         filterx_literal_new(filterx_object_ref(foo)));
  cr_assert_not(filterx_expr_unset(get_subscript));
  cr_assert(strstr(filterx_eval_get_last_error(), "readonly"));
  filterx_eval_clear_errors();
  filterx_expr_unref(get_subscript);


  FilterXExpr *inner = filterx_setattr_new(filterx_getattr_new(filterx_expr_ref(literal), "foo"),
                                           "bar",
                                           filterx_literal_new(filterx_object_ref(bar)));
  cr_assert_not(filterx_expr_eval(inner));
  cr_assert(strstr(filterx_eval_get_last_error(), "readonly"));
  filterx_eval_clear_errors();
  filterx_expr_unref(inner);


  filterx_expr_unref(literal);
  filterx_object_unref(bar);
  filterx_object_unref(foo);
}

static void
setup(void)
{
  app_startup();
  init_template_tests();
  init_libtest_filterx();
}

static void
teardown(void)
{
  deinit_libtest_filterx();
  scratch_buffers_explicit_gc();
  deinit_template_tests();
  app_shutdown();
}

TestSuite(filterx_expr, .init = setup, .fini = teardown);
