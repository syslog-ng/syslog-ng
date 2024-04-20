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
#include "filterx/object-datetime.h"
#include "filterx/object-primitive.h"
#include "filterx/object-null.h"
#include "filterx/object-string.h"
#include "apphook.h"
#include "filterx-lib.h"
#include "scratch-buffers.h"
#include "filterx/expr-literal.h"
#include "filterx/expr-function.h"

static void
assert_object_json_equals(FilterXObject *obj, const gchar *expected_json_repr)
{
  struct json_object *jso = NULL;

  cr_assert(filterx_object_map_to_json(obj, &jso) == TRUE, "error mapping to json, expected json was: %s",
            expected_json_repr);
  const gchar *json_repr = json_object_to_json_string_ext(jso, JSON_C_TO_STRING_PLAIN);
  cr_assert_str_eq(json_repr, expected_json_repr);
  json_object_put(jso);
}

Test(filterx_datetime, test_filterx_object_datetime_marshals_to_the_stored_values)
{
  UnixTime ut = { .ut_sec = 1701350398, .ut_usec = 123000, .ut_gmtoff = 3600 };

  FilterXObject *fobj = filterx_datetime_new(&ut);
  assert_marshaled_object(fobj, "1701350398.123000+01:00", LM_VT_DATETIME);
  filterx_object_unref(fobj);
}

Test(filterx_datetime, test_filterx_object_datetime_maps_to_the_right_json_value)
{
  UnixTime ut = { .ut_sec = 1701350398, .ut_usec = 123000, .ut_gmtoff = 3600 };
  FilterXObject *fobj = filterx_datetime_new(&ut);
  assert_object_json_equals(fobj, "\"1701350398.123000+01:00\"");
  filterx_object_unref(fobj);
}

Test(filterx_datetime, test_filterx_datetime_typecast_null_args)
{
  GPtrArray *args = NULL;

  FilterXObject *obj = filterx_typecast_datetime(args);
  cr_assert_null(obj);
}

Test(filterx_datetime, test_filterx_datetime_typecast_empty_args)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);

  FilterXObject *obj = filterx_typecast_datetime(args);
  cr_assert_null(obj);

  g_ptr_array_free(args, TRUE);
}

Test(filterx_datetime, test_filterx_datetime_typecast_null_arg)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  g_ptr_array_add(args, NULL);

  FilterXObject *obj = filterx_typecast_datetime(args);
  cr_assert_null(obj);

  g_ptr_array_free(args, TRUE);
}

Test(filterx_datetime, test_filterx_datetime_typecast_null_object_arg)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_null_new();
  g_ptr_array_add(args, in);

  FilterXObject *obj = filterx_typecast_datetime(args);
  cr_assert_null(obj);

  g_ptr_array_free(args, TRUE);
}

Test(filterx_datetime, test_filterx_datetime_typecast_from_int)
{
  // integer representation expected to be microsecond precision
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_integer_new(1710762325395194);
  g_ptr_array_add(args, in);

  FilterXObject *obj = filterx_typecast_datetime(args);
  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(datetime)));

  UnixTime ut_expected = { .ut_sec = 1710762325, .ut_usec = 395194, .ut_gmtoff = 0 };

  UnixTime ut = filterx_datetime_get_value(obj);
  cr_assert(memcmp(&ut_expected, &ut, sizeof(UnixTime)) == 0);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}

Test(filterx_datetime, test_filterx_datetime_typecast_from_double)
{
  // double representation expected to be second precision
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_double_new(1710762325.395194);
  g_ptr_array_add(args, in);

  FilterXObject *obj = filterx_typecast_datetime(args);
  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(datetime)));

  UnixTime ut_expected = { .ut_sec = 1710762325, .ut_usec = 395194, .ut_gmtoff = 0 };

  UnixTime ut = filterx_datetime_get_value(obj);
  cr_assert(memcmp(&ut_expected, &ut, sizeof(UnixTime)) == 0);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}

Test(filterx_datetime, test_filterx_datetime_typecast_from_string)
{
  // string representation expected to be rfc3339 standard
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_string_new("2024-03-18T12:34:00Z", -1);
  g_ptr_array_add(args, in);

  FilterXObject *obj = filterx_typecast_datetime(args);
  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(datetime)));

  UnixTime ut_expected = { .ut_sec = 1710765240, .ut_usec = 0, .ut_gmtoff = 0 };

  UnixTime ut = filterx_datetime_get_value(obj);
  cr_assert(memcmp(&ut_expected, &ut, sizeof(UnixTime)) == 0);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}

Test(filterx_datetime, test_filterx_datetime_typecast_from_datetime)
{
  UnixTime ut = { .ut_sec = 1701350398, .ut_usec = 123000, .ut_gmtoff = 3600 };

  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_datetime_new(&ut);
  g_ptr_array_add(args, in);

  FilterXObject *obj = filterx_typecast_datetime(args);

  cr_assert_eq(in, obj);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}

Test(filterx_datetime, test_filterx_datetime_repr)
{
  const gchar *test_time_str = "2024-03-18T12:34:13+234";
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_string_new(test_time_str, -1);
  g_ptr_array_add(args, in);

  FilterXObject *obj = filterx_typecast_datetime(args);
  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(datetime)));

  GString *repr = scratch_buffers_alloc();

  cr_assert(filterx_object_repr(in, repr));
  cr_assert_str_eq(test_time_str, repr->str);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}

Test(filterx_datetime, test_filterx_datetime_repr_isodate_Z)
{
  const gchar *test_time_str = "2024-03-18T12:34:00Z";
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_string_new(test_time_str, -1);
  g_ptr_array_add(args, in);

  FilterXObject *obj = filterx_typecast_datetime(args);
  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(datetime)));

  GString *repr = scratch_buffers_alloc();

  cr_assert(filterx_object_repr(in, repr));
  cr_assert_str_eq(test_time_str, repr->str);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}

Test(filterx_datetime, test_filterx_datetime_strptime_with_null_args)
{
  FilterXObject *obj = filterx_datetime_strptime(NULL);
  cr_assert_null(obj);

  filterx_object_unref(obj);
}

Test(filterx_datetime, test_filterx_datetime_strptime_without_args)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);

  FilterXObject *obj = filterx_datetime_strptime(args);
  cr_assert_null(obj);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}


Test(filterx_datetime, test_filterx_datetime_strptime_without_timefmt)
{
  const gchar *test_time_str = "2024-04-08T10:11:12Z";
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_string_new(test_time_str, -1);
  g_ptr_array_add(args, in);

  FilterXObject *obj = filterx_datetime_strptime(args);
  cr_assert_null(obj);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}

Test(filterx_datetime, test_filterx_datetime_strptime_non_matching_timefmt)
{
  const gchar *test_time_str = "2024-04-08T10:11:12Z";
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_string_new(test_time_str, -1);
  g_ptr_array_add(args, in);

  FilterXObject *time_fmt = filterx_string_new("non matching timefmt", -1);
  g_ptr_array_add(args, time_fmt);

  FilterXObject *obj = filterx_datetime_strptime(args);
  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(null)));

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}

Test(filterx_datetime, test_filterx_datetime_strptime_matching_timefmt)
{
  const gchar *test_time_str = "2024-04-08T10:11:12Z";
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_string_new(test_time_str, -1);
  g_ptr_array_add(args, in);

  FilterXObject *time_fmt = filterx_string_new(datefmt_isodate, -1);
  g_ptr_array_add(args, time_fmt);

  FilterXObject *obj = filterx_datetime_strptime(args);
  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(datetime)));

  GString *repr = scratch_buffers_alloc();

  cr_assert(filterx_object_repr(in, repr));
  cr_assert_str_eq(test_time_str, repr->str);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}

Test(filterx_datetime, test_filterx_datetime_strptime_matching_nth_timefmt)
{
  const gchar *test_time_str = "2024-04-08T10:11:12+01:00";
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_string_new(test_time_str, -1);
  g_ptr_array_add(args, in);

  FilterXObject *bad_fmt1 = filterx_string_new("bad format 1", -1);
  g_ptr_array_add(args, bad_fmt1);

  FilterXObject *bad_fmt2 = filterx_string_new("bad format 2", -1);
  g_ptr_array_add(args, bad_fmt2);

  FilterXObject *time_fmt = filterx_string_new(datefmt_isodate, -1);
  g_ptr_array_add(args, time_fmt);

  FilterXObject *obj = filterx_datetime_strptime(args);
  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(datetime)));

  GString *repr = scratch_buffers_alloc();

  cr_assert(filterx_object_repr(in, repr));
  cr_assert_str_eq(test_time_str, repr->str);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}

Test(filterx_datetime, test_filterx_datetime_strptime_non_matching_nth_timefmt)
{
  const gchar *test_time_str = "2024-04-08T10:11:12Z";
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_string_new(test_time_str, -1);
  g_ptr_array_add(args, in);

  FilterXObject *bad_fmt1 = filterx_string_new("bad format 1", -1);
  g_ptr_array_add(args, bad_fmt1);

  FilterXObject *bad_fmt2 = filterx_string_new("bad format 2", -1);
  g_ptr_array_add(args, bad_fmt2);

  FilterXObject *time_fmt = filterx_string_new("non matching fmt", -1);
  g_ptr_array_add(args, time_fmt);

  FilterXObject *obj = filterx_datetime_strptime(args);
  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(null)));

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}

Test(filterx_datetime, test_filterx_datetime_strptime_invalid_arg_type)
{
  const gchar *test_time_str = "2024-04-08T10:11:12Z";
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_string_new(test_time_str, -1);
  g_ptr_array_add(args, in);

  FilterXObject *bad_fmt1 = filterx_integer_new(1337);
  g_ptr_array_add(args, bad_fmt1);

  FilterXObject *time_fmt = filterx_string_new(datefmt_isodate, -1);
  g_ptr_array_add(args, time_fmt);

  FilterXObject *obj = filterx_datetime_strptime(args);
  cr_assert_null(obj);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}

Test(filterx_datetime, test_filterx_datetime_strptime_matching_nth_timefmt_call_as_filterx_func)
{
  const gchar *test_time_str = "2024-04-08T10:11:12+01:00";
  FilterXObject *in = filterx_string_new(test_time_str, -1);
  FilterXObject *bad_fmt1 = filterx_string_new("bad format 1", -1);
  FilterXObject *bad_fmt2 = filterx_string_new("bad format 2", -1);
  FilterXObject *time_fmt = filterx_string_new(datefmt_isodate, -1);

  GList *args = NULL;
  args = g_list_append(args, filterx_literal_new(in));
  args = g_list_append(args, filterx_literal_new(bad_fmt1));
  args = g_list_append(args, filterx_literal_new(bad_fmt2));
  args = g_list_append(args, filterx_literal_new(time_fmt));
  FilterXExpr *func = filterx_function_new("test_strptime", args, filterx_datetime_strptime);
  cr_assert_not_null(func);
  FilterXObject *res = filterx_expr_eval(func);
  cr_assert_not_null(res);
  cr_assert(filterx_object_is_type(res, &FILTERX_TYPE_NAME(datetime)));
  GString *repr = scratch_buffers_alloc();
  cr_assert(filterx_object_repr(in, repr));
  cr_assert_str_eq(test_time_str, repr->str);
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

TestSuite(filterx_datetime, .init = setup, .fini = teardown);
