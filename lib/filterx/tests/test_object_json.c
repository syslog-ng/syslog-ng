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
#include "libtest/filterx-lib.h"

#include "filterx/object-json.h"
#include "filterx/object-string.h"
#include "filterx/object-message-value.h"
#include "filterx/expr-function.h"
#include "apphook.h"

Test(filterx_json, test_filterx_object_json_from_repr)
{
  FilterXObject *fobj;

  fobj = filterx_json_new_from_repr("{\"foo\": \"foovalue\"}", -1);
  cr_assert(filterx_object_is_type(fobj, &FILTERX_TYPE_NAME(json_object)));
  assert_object_json_equals(fobj, "{\"foo\":\"foovalue\"}");
  assert_marshaled_object(fobj, "{\"foo\":\"foovalue\"}", LM_VT_JSON);
  filterx_object_unref(fobj);

  fobj = filterx_json_new_from_repr("[\"foo\", \"bar\"]", -1);
  cr_assert(filterx_object_is_type(fobj, &FILTERX_TYPE_NAME(json_array)));
  assert_object_json_equals(fobj, "[\"foo\",\"bar\"]");
  assert_marshaled_object(fobj, "foo,bar", LM_VT_LIST);
  filterx_object_unref(fobj);

  fobj = filterx_json_new_from_repr("[1, 2]", -1);
  cr_assert(filterx_object_is_type(fobj, &FILTERX_TYPE_NAME(json_array)));
  assert_object_json_equals(fobj, "[1,2]");
  assert_marshaled_object(fobj, "[1,2]", LM_VT_JSON);
  filterx_object_unref(fobj);

  fobj = filterx_json_array_new_from_syslog_ng_list("\"foo\",bar", -1);
  cr_assert(filterx_object_is_type(fobj, &FILTERX_TYPE_NAME(json_array)));
  assert_object_json_equals(fobj, "[\"foo\",\"bar\"]");
  assert_marshaled_object(fobj, "foo,bar", LM_VT_LIST);
  filterx_object_unref(fobj);

  fobj = filterx_json_array_new_from_syslog_ng_list("1,2", -1);
  cr_assert(filterx_object_is_type(fobj, &FILTERX_TYPE_NAME(json_array)));
  assert_object_json_equals(fobj, "[\"1\",\"2\"]");
  assert_marshaled_object(fobj, "1,2", LM_VT_LIST);
  filterx_object_unref(fobj);
}

static FilterXObject *
_exec_func(FilterXSimpleFunctionProto func, FilterXObject *arg)
{
  if (!arg)
    return func(NULL);

  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  g_ptr_array_add(args, arg);
  FilterXObject *result = func(args);
  g_ptr_array_unref(args);
  return result;
}

static FilterXObject *
_exec_json_func(FilterXObject *arg)
{
  return _exec_func(filterx_json_new_from_args, arg);
}

static FilterXObject *
_exec_json_array_func(FilterXObject *arg)
{
  return _exec_func(filterx_json_array_new_from_args, arg);
}

Test(filterx_json, test_json_function)
{
  FilterXObject *fobj;

  fobj = _exec_json_func(NULL);
  cr_assert(filterx_object_is_type(fobj, &FILTERX_TYPE_NAME(json_object)));
  assert_object_json_equals(fobj, "{}");
  filterx_object_unref(fobj);

  fobj = _exec_json_func(filterx_string_new("{\"foo\": 1}", -1));
  cr_assert(filterx_object_is_type(fobj, &FILTERX_TYPE_NAME(json_object)));
  assert_object_json_equals(fobj, "{\"foo\":1}");
  filterx_object_unref(fobj);

  fobj = _exec_json_func(filterx_message_value_new("{\"foo\": 1}", -1, LM_VT_JSON));
  cr_assert(filterx_object_is_type(fobj, &FILTERX_TYPE_NAME(json_object)));
  assert_object_json_equals(fobj, "{\"foo\":1}");
  filterx_object_unref(fobj);

  fobj = _exec_json_func(filterx_string_new("[1, 2]", -1));
  cr_assert(filterx_object_is_type(fobj, &FILTERX_TYPE_NAME(json_array)));
  assert_object_json_equals(fobj, "[1,2]");
  filterx_object_unref(fobj);

  fobj = _exec_json_func(filterx_message_value_new("[1, 2]", -1, LM_VT_JSON));
  cr_assert(filterx_object_is_type(fobj, &FILTERX_TYPE_NAME(json_array)));
  assert_object_json_equals(fobj, "[1,2]");
  filterx_object_unref(fobj);

  fobj = _exec_json_func(filterx_message_value_new("foo,bar", -1, LM_VT_LIST));
  cr_assert(filterx_object_is_type(fobj, &FILTERX_TYPE_NAME(json_array)));
  assert_object_json_equals(fobj, "[\"foo\",\"bar\"]");
  filterx_object_unref(fobj);

  fobj = _exec_json_func(filterx_json_object_new_from_repr("{\"foo\": 1}", -1));
  cr_assert(filterx_object_is_type(fobj, &FILTERX_TYPE_NAME(json_object)));
  assert_object_json_equals(fobj, "{\"foo\":1}");
  filterx_object_unref(fobj);

  fobj = _exec_json_func(filterx_json_array_new_from_repr("[1, 2]", -1));
  cr_assert(filterx_object_is_type(fobj, &FILTERX_TYPE_NAME(json_array)));
  assert_object_json_equals(fobj, "[1,2]");
  filterx_object_unref(fobj);
}

Test(filterx_json, test_json_array_function)
{
  FilterXObject *fobj;

  fobj = _exec_json_array_func(NULL);
  cr_assert(filterx_object_is_type(fobj, &FILTERX_TYPE_NAME(json_array)));
  assert_object_json_equals(fobj, "[]");
  filterx_object_unref(fobj);

  fobj = _exec_json_array_func(filterx_string_new("[1, 2]", -1));
  cr_assert(filterx_object_is_type(fobj, &FILTERX_TYPE_NAME(json_array)));
  assert_object_json_equals(fobj, "[1,2]");
  filterx_object_unref(fobj);

  fobj = _exec_json_array_func(filterx_message_value_new("[1, 2]", -1, LM_VT_JSON));
  cr_assert(filterx_object_is_type(fobj, &FILTERX_TYPE_NAME(json_array)));
  assert_object_json_equals(fobj, "[1,2]");
  filterx_object_unref(fobj);

  fobj = _exec_json_array_func(filterx_message_value_new("foo,bar", -1, LM_VT_LIST));
  cr_assert(filterx_object_is_type(fobj, &FILTERX_TYPE_NAME(json_array)));
  assert_object_json_equals(fobj, "[\"foo\",\"bar\"]");
  filterx_object_unref(fobj);

  fobj = _exec_json_array_func(filterx_json_array_new_from_repr("[1, 2]", -1));
  cr_assert(filterx_object_is_type(fobj, &FILTERX_TYPE_NAME(json_array)));
  assert_object_json_equals(fobj, "[1,2]");
  filterx_object_unref(fobj);
}

static void
setup(void)
{
  app_startup();
}

static void
teardown(void)
{
  app_shutdown();
}

TestSuite(filterx_json, .init = setup, .fini = teardown);
