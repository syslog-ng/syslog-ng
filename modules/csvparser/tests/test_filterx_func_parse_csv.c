/*
 * Copyright (c) 2024 shifter
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

#include "apphook.h"
#include "scratch-buffers.h"

#include "filterx/object-string.h"
#include "filterx/object-null.h"
#include "filterx/expr-literal.h"
#include "filterx/object-json.h"
#include "filterx-func-parse-csv.h"
#include "filterx/object-list-interface.h"
#include "scanner/csv-scanner/csv-scanner.h"
#include "filterx/object-primitive.h"

static FilterXObject *
_generate_column_list(const gchar *column_name, ...)
{
  FilterXObject *result = filterx_json_array_new_empty();

  va_list args;
  va_start(args, column_name);

  const gchar *next_column = column_name;
  while (next_column != NULL)
    {
      FilterXObject *col_name = filterx_string_new(next_column, -1);
      cr_assert(filterx_list_append(result, &col_name));
      filterx_object_unref(col_name);
      next_column = va_arg(args, const gchar *);
    }

  va_end(args);
  va_start(args, column_name);

  return result;
}

Test(filterx_func_parse_csv, test_helper_generate_column_list_empty)
{
  FilterXObject *col_names = _generate_column_list(NULL);
  cr_assert_not_null(col_names);

  GString *repr = scratch_buffers_alloc();
  LogMessageValueType lmvt;

  cr_assert(filterx_object_marshal(col_names, repr, &lmvt));
  cr_assert_str_eq(repr->str, "");
  filterx_object_unref(col_names);
}

Test(filterx_func_parse_csv, test_helper_generate_column_list)
{
  FilterXObject *col_names = _generate_column_list("1st", NULL);
  cr_assert_not_null(col_names);

  GString *repr = scratch_buffers_alloc();
  LogMessageValueType lmvt;

  cr_assert(filterx_object_marshal(col_names, repr, &lmvt));
  cr_assert_str_eq(repr->str, "1st");
  filterx_object_unref(col_names);
}

Test(filterx_func_parse_csv, test_helper_generate_column_list_multiple_elts)
{
  FilterXObject *col_names = _generate_column_list("1st", "2nd", "3rd", NULL);
  cr_assert_not_null(col_names);

  GString *repr = scratch_buffers_alloc();
  LogMessageValueType lmvt;

  cr_assert(filterx_object_marshal(col_names, repr, &lmvt));
  cr_assert_str_eq(repr->str, "1st,2nd,3rd");
  filterx_object_unref(col_names);
}

Test(filterx_func_parse_csv, test_empty_args_error)
{
  GError *err = NULL;
  FilterXExpr *func = filterx_function_parse_csv_new("test", NULL, &err);

  cr_assert_null(func);
  cr_assert_not_null(err);
  cr_assert(strstr(err->message, FILTERX_FUNC_PARSE_CSV_USAGE) != NULL);
  g_error_free(err);
}


Test(filterx_func_parse_csv, test_skipped_opts_causes_default_behaviour)
{
  GList *args = NULL;
  args = g_list_append(args, filterx_literal_new(filterx_string_new("foo bar baz tik tak toe", -1)));

  GError *err = NULL;
  FilterXExpr *func = filterx_function_parse_csv_new("test", args, &err);

  cr_assert_null(err);

  FilterXObject *obj = filterx_expr_eval(func);

  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(json_array)));

  GString *repr = scratch_buffers_alloc();

  LogMessageValueType lmvt;
  cr_assert(filterx_object_marshal(obj, repr, &lmvt));

  cr_assert_str_eq(repr->str, "foo,bar,baz,tik,tak,toe");
  filterx_expr_unref(func);
  filterx_object_unref(obj);
  g_error_free(err);
}

Test(filterx_func_parse_csv, test_columns_optional_argument_is_nullable)
{
  GList *args = NULL;
  args = g_list_append(args, filterx_literal_new(filterx_string_new("foo bar baz tik tak toe", -1)));
  args = g_list_append(args, filterx_literal_new(filterx_null_new()));

  GError *err = NULL;
  FilterXExpr *func = filterx_function_parse_csv_new("test", args, &err);

  cr_assert_null(err);

  FilterXObject *obj = filterx_expr_eval(func);

  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(json_array)));

  GString *repr = scratch_buffers_alloc();

  LogMessageValueType lmvt;
  cr_assert(filterx_object_marshal(obj, repr, &lmvt));

  cr_assert_str_eq(repr->str, "foo,bar,baz,tik,tak,toe");
  filterx_expr_unref(func);
  filterx_object_unref(obj);
  g_error_free(err);
}

Test(filterx_func_parse_csv, test_set_optional_first_argument_column_names)
{
  GList *args = NULL;
  args = g_list_append(args, filterx_literal_new(filterx_string_new("foo bar baz", -1)));
  FilterXObject *col_names = _generate_column_list("1st", "2nd", "3rd", NULL);
  args = g_list_append(args, filterx_literal_new(col_names));

  GError *err = NULL;
  FilterXExpr *func = filterx_function_parse_csv_new("test", args, &err);

  cr_assert_null(err);

  FilterXObject *obj = filterx_expr_eval(func);

  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(json_object)));

  GString *repr = scratch_buffers_alloc();

  LogMessageValueType lmvt;
  cr_assert(filterx_object_marshal(obj, repr, &lmvt));

  cr_assert_str_eq(repr->str, "{\"1st\":\"foo\",\"2nd\":\"bar\",\"3rd\":\"baz\"}");
  filterx_expr_unref(func);
  filterx_object_unref(obj);
  g_error_free(err);
}

Test(filterx_func_parse_csv, test_column_names_sets_expected_column_size_additional_columns_dropped)
{
  GList *args = NULL;
  args = g_list_append(args, filterx_literal_new(filterx_string_new("foo bar baz more columns we did not expect", -1)));
  FilterXObject *col_names = _generate_column_list("1st", "2nd", "3rd", NULL); // sets expected column size 3
  args = g_list_append(args, filterx_literal_new(col_names));

  GError *err = NULL;
  FilterXExpr *func = filterx_function_parse_csv_new("test", args, &err);

  cr_assert_null(err);

  FilterXObject *obj = filterx_expr_eval(func);

  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(json_object)));

  GString *repr = scratch_buffers_alloc();

  LogMessageValueType lmvt;
  cr_assert(filterx_object_marshal(obj, repr, &lmvt));

  cr_assert_str_eq(repr->str, "{\"1st\":\"foo\",\"2nd\":\"bar\",\"3rd\":\"baz\"}");
  filterx_expr_unref(func);
  filterx_object_unref(obj);
  g_error_free(err);
}

Test(filterx_func_parse_csv, test_optional_argument_delimiters)
{
  GList *args = NULL;
  args = g_list_append(args, filterx_literal_new(filterx_string_new("foo bar+baz;tik|tak:toe", -1)));
  args = g_list_append(args, filterx_literal_new(filterx_null_new())); // columns
  args = g_list_append(args, filterx_literal_new(filterx_string_new(" +;", -1)));

  GError *err = NULL;
  FilterXExpr *func = filterx_function_parse_csv_new("test", args, &err);

  cr_assert_null(err);

  FilterXObject *obj = filterx_expr_eval(func);

  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(json_array)));

  GString *repr = scratch_buffers_alloc();

  LogMessageValueType lmvt;
  cr_assert(filterx_object_marshal(obj, repr, &lmvt));

  cr_assert_str_eq(repr->str, "foo,bar,baz,tik|tak:toe");
  filterx_expr_unref(func);
  filterx_object_unref(obj);
  g_error_free(err);
}

Test(filterx_func_parse_csv, test_optional_argument_delimiters_is_nullable)
{
  GList *args = NULL;
  args = g_list_append(args, filterx_literal_new(filterx_string_new("foo bar+baz;tik|tak:toe", -1)));
  args = g_list_append(args, filterx_literal_new(filterx_null_new())); // columns
  args = g_list_append(args, filterx_literal_new(filterx_null_new())); // delimiter

  GError *err = NULL;
  FilterXExpr *func = filterx_function_parse_csv_new("test", args, &err);

  cr_assert_null(err);

  FilterXObject *obj = filterx_expr_eval(func);

  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(json_array)));

  GString *repr = scratch_buffers_alloc();

  LogMessageValueType lmvt;
  cr_assert(filterx_object_marshal(obj, repr, &lmvt));

  cr_assert_str_eq(repr->str, "foo,bar+baz;tik|tak:toe");
  filterx_expr_unref(func);
  filterx_object_unref(obj);
  g_error_free(err);
}

Test(filterx_func_parse_csv, test_optional_argument_dialect)
{
  GList *args = NULL;
  args = g_list_append(args, filterx_literal_new(filterx_string_new("\"PTHREAD \\\"support initialized\"", -1)));
  args = g_list_append(args, filterx_literal_new(filterx_null_new())); // columns
  args = g_list_append(args, filterx_literal_new(filterx_null_new())); // delimiter
  args = g_list_append(args, filterx_literal_new(filterx_string_new("CSV_SCANNER_ESCAPE_BACKSLASH", -1)));

  GError *err = NULL;
  FilterXExpr *func = filterx_function_parse_csv_new("test", args, &err);
  cr_assert_null(err);

  FilterXObject *obj = filterx_expr_eval(func);

  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(json_array)));

  GString *repr = scratch_buffers_alloc();

  LogMessageValueType lmvt;
  cr_assert(filterx_object_marshal(obj, repr, &lmvt));

  cr_assert_str_eq(repr->str, "'PTHREAD \"support initialized'");
  filterx_expr_unref(func);
  filterx_object_unref(obj);
  g_error_free(err);
}

Test(filterx_func_parse_csv, test_optional_argument_dialect_is_nullable)
{
  GList *args = NULL;
  args = g_list_append(args, filterx_literal_new(filterx_string_new("\"PTHREAD \\\"support initialized\"", -1)));
  args = g_list_append(args, filterx_literal_new(filterx_null_new())); // columns
  args = g_list_append(args, filterx_literal_new(filterx_null_new())); // delimiter
  args = g_list_append(args, filterx_literal_new(filterx_null_new())); // dialect

  GError *err = NULL;
  FilterXExpr *func = filterx_function_parse_csv_new("test", args, &err);
  cr_assert_null(err);

  FilterXObject *obj = filterx_expr_eval(func);

  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(json_array)));

  GString *repr = scratch_buffers_alloc();

  LogMessageValueType lmvt;
  cr_assert(filterx_object_marshal(obj, repr, &lmvt));

  cr_assert_str_eq(repr->str, "\"PTHREAD \\\\support\",'initialized\"'");
  filterx_expr_unref(func);
  filterx_object_unref(obj);
  g_error_free(err);
}

Test(filterx_func_parse_csv, test_optional_argument_flag_greedy)
{
  GList *args = NULL;
  args = g_list_append(args, filterx_literal_new(filterx_string_new("foo bar baz tik tak toe", -1)));
  args = g_list_append(args, filterx_literal_new(_generate_column_list("1st", "2nd", "3rd", "rest", NULL))); // columns
  args = g_list_append(args, filterx_literal_new(filterx_null_new())); // delimiter
  args = g_list_append(args, filterx_literal_new(filterx_null_new())); // dialect
  args = g_list_append(args, filterx_literal_new(filterx_boolean_new(TRUE))); // greedy

  GError *err = NULL;
  FilterXExpr *func = filterx_function_parse_csv_new("test", args, &err);
  cr_assert_null(err);

  FilterXObject *obj = filterx_expr_eval(func);

  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(json_object)));

  GString *repr = scratch_buffers_alloc();

  LogMessageValueType lmvt;
  cr_assert(filterx_object_marshal(obj, repr, &lmvt));

  cr_assert_str_eq(repr->str, "{\"1st\":\"foo\",\"2nd\":\"bar\",\"3rd\":\"baz\",\"rest\":\"tik tak toe\"}");
  filterx_expr_unref(func);
  filterx_object_unref(obj);
  g_error_free(err);
}

Test(filterx_func_parse_csv, test_optional_argument_flag_non_greedy)
{
  GList *args = NULL;
  args = g_list_append(args, filterx_literal_new(filterx_string_new("foo bar baz tik tak toe", -1)));
  args = g_list_append(args, filterx_literal_new(_generate_column_list("1st", "2nd", "3rd", "rest", NULL))); // columns
  args = g_list_append(args, filterx_literal_new(filterx_null_new())); // delimiter
  args = g_list_append(args, filterx_literal_new(filterx_null_new())); // dialect
  args = g_list_append(args, filterx_literal_new(filterx_boolean_new(FALSE))); // greedy

  GError *err = NULL;
  FilterXExpr *func = filterx_function_parse_csv_new("test", args, &err);
  cr_assert_null(err);

  FilterXObject *obj = filterx_expr_eval(func);

  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(json_object)));

  GString *repr = scratch_buffers_alloc();

  LogMessageValueType lmvt;
  cr_assert(filterx_object_marshal(obj, repr, &lmvt));

  cr_assert_str_eq(repr->str, "{\"1st\":\"foo\",\"2nd\":\"bar\",\"3rd\":\"baz\",\"rest\":\"tik\"}");
  filterx_expr_unref(func);
  filterx_object_unref(obj);
  g_error_free(err);
}

Test(filterx_func_parse_csv, test_optional_argument_flag_strip_whitespace)
{
  GList *args = NULL;
  args = g_list_append(args, filterx_literal_new(filterx_string_new("  foo ,    bar  , baz   ,    tik tak toe", -1)));
  args = g_list_append(args, filterx_literal_new(filterx_null_new())); // columns
  args = g_list_append(args, filterx_literal_new(filterx_string_new(",", -1))); // delimiter
  args = g_list_append(args, filterx_literal_new(filterx_null_new())); // dialect
  args = g_list_append(args, filterx_literal_new(filterx_null_new())); // greedy
  args = g_list_append(args, filterx_literal_new(filterx_boolean_new(TRUE))); // strip_whitespace

  GError *err = NULL;
  FilterXExpr *func = filterx_function_parse_csv_new("test", args, &err);
  cr_assert_null(err);

  FilterXObject *obj = filterx_expr_eval(func);

  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(json_array)));

  GString *repr = scratch_buffers_alloc();

  LogMessageValueType lmvt;
  cr_assert(filterx_object_marshal(obj, repr, &lmvt));

  cr_assert_str_eq(repr->str, "foo,bar,baz,\"tik tak toe\"");
  filterx_expr_unref(func);
  filterx_object_unref(obj);
  g_error_free(err);
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

TestSuite(filterx_func_parse_csv, .init = setup, .fini = teardown);
