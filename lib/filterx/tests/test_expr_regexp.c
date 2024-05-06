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

#include "filterx/expr-regexp.h"
#include "filterx/expr-literal.h"
#include "filterx/object-string.h"
#include "filterx/object-primitive.h"
#include "filterx/object-dict-interface.h"
#include "filterx/object-list-interface.h"
#include "apphook.h"

static gboolean
_check_match(const gchar *lhs, const gchar *pattern)
{
  FilterXExpr *expr = filterx_expr_regexp_match_new(filterx_literal_new(filterx_string_new(lhs, -1)), pattern);

  FilterXObject *result_obj = filterx_expr_eval(expr);
  cr_assert(result_obj);
  gboolean result;
  cr_assert(filterx_boolean_unwrap(result_obj, &result));

  filterx_expr_unref(expr);
  filterx_object_unref(result_obj);

  return result;
}

static void
_assert_match(const gchar *lhs, const gchar *pattern)
{
  cr_assert(_check_match(lhs, pattern), "regexp should match. lhs: %s pattern: %s", lhs, pattern);
}

static void
_assert_not_match(const gchar *lhs, const gchar *pattern)
{
  cr_assert_not(_check_match(lhs, pattern), "regexp should not match. lhs: %s pattern: %s", lhs, pattern);
}

static void
_assert_match_init_error(const gchar *lhs, const gchar *pattern)
{
  cr_assert_not(filterx_expr_regexp_match_new(filterx_literal_new(filterx_string_new(lhs, -1)), pattern));
}

Test(filterx_expr_regexp, regexp_match)
{
  _assert_match("foo", "(?<key>foo)");
  _assert_match("foo", "(?<key>foo)|(?<key>bar)");
  _assert_not_match("abc", "Abc");
  _assert_not_match("abc", "(?<key>Abc)");
  _assert_match_init_error("abc", "(");
}

static FilterXObject *
_search(const gchar *lhs, const gchar *pattern)
{
  FilterXExpr *expr = filterx_expr_regexp_search_generator_new(filterx_literal_new(filterx_string_new(lhs, -1)), pattern);
  FilterXExpr *parent_fillable_expr_new = filterx_literal_new(filterx_test_dict_new());
  FilterXExpr *cc_expr = filterx_generator_create_container_new(expr, parent_fillable_expr_new);
  FilterXExpr *fillable_expr = filterx_literal_new(filterx_expr_eval(cc_expr));
  filterx_generator_set_fillable(expr, fillable_expr);

  FilterXObject *result_obj = filterx_expr_eval(expr);
  cr_assert(result_obj);
  cr_assert(filterx_object_truthy(result_obj));

  FilterXObject *fillable = filterx_expr_eval(fillable_expr);
  cr_assert(fillable);

  filterx_object_unref(result_obj);
  filterx_expr_unref(cc_expr);
  filterx_expr_unref(parent_fillable_expr_new);
  filterx_expr_unref(expr);

  return fillable;
}

static void
_search_with_fillable(const gchar *lhs, const gchar *pattern, FilterXObject *fillable)
{
  FilterXExpr *expr = filterx_expr_regexp_search_generator_new(filterx_literal_new(filterx_string_new(lhs, -1)), pattern);
  filterx_generator_set_fillable(expr, filterx_literal_new(filterx_object_ref(fillable)));

  FilterXObject *result_obj = filterx_expr_eval(expr);
  cr_assert(result_obj);
  cr_assert(filterx_object_truthy(result_obj));

  filterx_object_unref(result_obj);
  filterx_expr_unref(expr);
}

static void
_assert_search_init_error(const gchar *lhs, const gchar *pattern)
{
  cr_assert_not(filterx_expr_regexp_search_generator_new(filterx_literal_new(filterx_string_new(lhs, -1)), pattern));
}

static void
_assert_len(FilterXObject *obj, guint64 expected_len)
{
  guint64 len;
  cr_assert(filterx_object_len(obj, &len));
  cr_assert_eq(len, expected_len, "len mismatch. expected: %" G_GUINT64_FORMAT " actual: %" G_GUINT64_FORMAT,
               expected_len, len);
}

static void
_assert_list_elem(FilterXObject *list, gint64 index, const gchar *expected_value)
{
  FilterXObject *elem = filterx_list_get_subscript(list, index);
  cr_assert(elem);

  const gchar *value = filterx_string_get_value(elem, NULL);
  cr_assert_str_eq(value, expected_value);

  filterx_object_unref(elem);
}

static void
_assert_dict_elem(FilterXObject *list, const gchar *key, const gchar *expected_value)
{
  FilterXObject *key_obj = filterx_string_new(key, -1);
  FilterXObject *elem = filterx_object_get_subscript(list, key_obj);
  cr_assert(elem);

  const gchar *value = filterx_string_get_value(elem, NULL);
  cr_assert_str_eq(value, expected_value);

  filterx_object_unref(key_obj);
  filterx_object_unref(elem);
}

Test(filterx_expr_regexp, regexp_search_unnamed)
{
  FilterXObject *result = _search("foobarbaz", "(foo)(bar)(baz)");
  cr_assert(filterx_object_is_type(result, &FILTERX_TYPE_NAME(list)));
  _assert_len(result, 4);
  _assert_list_elem(result, 0, "foobarbaz");
  _assert_list_elem(result, 1, "foo");
  _assert_list_elem(result, 2, "bar");
  _assert_list_elem(result, 3, "baz");
  filterx_object_unref(result);
}

Test(filterx_expr_regexp, regexp_search_named)
{
  FilterXObject *result = _search("foobarbaz", "(?<first>foo)(?<second>bar)(?<third>baz)");
  cr_assert(filterx_object_is_type(result, &FILTERX_TYPE_NAME(dict)));
  _assert_len(result, 4);
  _assert_dict_elem(result, "0", "foobarbaz");
  _assert_dict_elem(result, "first", "foo");
  _assert_dict_elem(result, "second", "bar");
  _assert_dict_elem(result, "third", "baz");
  filterx_object_unref(result);
}

Test(filterx_expr_regexp, regexp_search_mixed)
{
  FilterXObject *result = _search("foobarbaz", "(?<first>foo)(bar)(?<third>baz)");
  cr_assert(filterx_object_is_type(result, &FILTERX_TYPE_NAME(dict)));
  _assert_len(result, 4);
  _assert_dict_elem(result, "0", "foobarbaz");
  _assert_dict_elem(result, "first", "foo");
  _assert_dict_elem(result, "2", "bar");
  _assert_dict_elem(result, "third", "baz");
  filterx_object_unref(result);
}

Test(filterx_expr_regexp, regexp_search_forced_list)
{
  FilterXObject *result = filterx_test_list_new();
  _search_with_fillable("foobarbaz", "(?<first>foo)(bar)(?<third>baz)", result);
  _assert_len(result, 4);
  _assert_list_elem(result, 0, "foobarbaz");
  _assert_list_elem(result, 1, "foo");
  _assert_list_elem(result, 2, "bar");
  _assert_list_elem(result, 3, "baz");
  filterx_object_unref(result);
}

Test(filterx_expr_regexp, regexp_search_forced_dict)
{
  FilterXObject *result = filterx_test_dict_new();
  _search_with_fillable("foobarbaz", "(foo)(bar)(baz)", result);
  _assert_len(result, 4);
  _assert_dict_elem(result, "0", "foobarbaz");
  _assert_dict_elem(result, "1", "foo");
  _assert_dict_elem(result, "2", "bar");
  _assert_dict_elem(result, "3", "baz");
  filterx_object_unref(result);
}

Test(filterx_expr_regexp, regexp_search_unnamed_no_match)
{
  FilterXObject *result = _search("foobarbaz", "(almafa)");
  cr_assert(filterx_object_is_type(result, &FILTERX_TYPE_NAME(list)));
  _assert_len(result, 0);
  filterx_object_unref(result);
}

Test(filterx_expr_regexp, regexp_search_named_no_match)
{
  FilterXObject *result = _search("foobarbaz", "(?<first>almafa)");
  cr_assert(filterx_object_is_type(result, &FILTERX_TYPE_NAME(dict)));
  _assert_len(result, 0);
  filterx_object_unref(result);
}

Test(filterx_expr_regexp, regexp_search_init_error)
{
  _assert_search_init_error("foobarbaz", "(");
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
  deinit_libtest_filterx();
  app_shutdown();
}

TestSuite(filterx_expr_regexp, .init = setup, .fini = teardown);
