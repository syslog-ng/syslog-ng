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

#include "filterx/object-message-value.h"
#include "apphook.h"
#include "scratch-buffers.h"

Test(filterx_message, test_filterx_object_message_marshals_to_the_stored_values)
{
  FilterXObject *fobj = filterx_message_value_new("True", 4, LM_VT_BOOLEAN);
  assert_marshaled_object(fobj, "True", LM_VT_BOOLEAN);
  filterx_object_unref(fobj);

  fobj = filterx_message_value_new("42", 2, LM_VT_INTEGER);
  assert_marshaled_object(fobj, "42", LM_VT_INTEGER);
  filterx_object_unref(fobj);

  fobj = filterx_message_value_new_ref(g_strdup("35.6"), 4, LM_VT_DOUBLE);
  assert_marshaled_object(fobj, "35.6", LM_VT_DOUBLE);
  filterx_object_unref(fobj);

  gchar borrowed_value[] = "string";

  fobj = filterx_message_value_new_borrowed(borrowed_value, -1, LM_VT_STRING);
  assert_marshaled_object(fobj, "string", LM_VT_STRING);
  borrowed_value[0]++;
  assert_marshaled_object(fobj, "ttring", LM_VT_STRING);
  filterx_object_unref(fobj);
}

Test(filterx_message, test_filterx_object_value_maps_to_the_right_json_value)
{
  FilterXObject *fobj = filterx_message_value_new("True", 4, LM_VT_BOOLEAN);
  assert_object_json_equals(fobj, "true");
  filterx_object_unref(fobj);

  fobj = filterx_message_value_new("42", 2, LM_VT_INTEGER);
  assert_object_json_equals(fobj, "42");
  filterx_object_unref(fobj);

  fobj = filterx_message_value_new_ref(g_strdup("36.0"), 4, LM_VT_DOUBLE);
  assert_object_json_equals(fobj, "36.0");
  filterx_object_unref(fobj);

  gchar borrowed_value[] = "string";

  fobj = filterx_message_value_new_borrowed(borrowed_value, -1, LM_VT_STRING);
  assert_object_json_equals(fobj, "\"string\"");
  borrowed_value[0]++;
  assert_object_json_equals(fobj, "\"ttring\"");
  filterx_object_unref(fobj);
}

Test(filterx_message, test_filterx_message_type_null_repr)
{
  FilterXObject *fobj = filterx_message_value_new(NULL, 0, LM_VT_NULL);
  GString *repr = scratch_buffers_alloc();
  cr_assert(filterx_object_repr(fobj, repr));
  cr_assert_str_eq("null", repr->str);
  filterx_object_unref(fobj);
}

Test(filterx_message, test_filterx_message_type_string_repr)
{
  gchar *test_str = "any string";

  FilterXObject *fobj = filterx_message_value_new(test_str, -1, LM_VT_STRING);
  GString *repr = scratch_buffers_alloc();
  cr_assert(filterx_object_repr(fobj, repr));
  cr_assert_str_eq(test_str, repr->str);
  filterx_object_unref(fobj);
}

Test(filterx_message, test_filterx_message_type_bytes_repr)
{
  gchar *test_str = "any bytes";

  FilterXObject *fobj = filterx_message_value_new(test_str, -1, LM_VT_BYTES);
  GString *repr = scratch_buffers_alloc();
  cr_assert(filterx_object_repr(fobj, repr));
  cr_assert_str_eq(test_str, repr->str);
  filterx_object_unref(fobj);
}

Test(filterx_message, test_filterx_message_type_protobuf_repr)
{
  gchar *test_str = "not a valid protobuf!";

  FilterXObject *fobj = filterx_message_value_new(test_str, -1, LM_VT_PROTOBUF);
  GString *repr = scratch_buffers_alloc();
  cr_assert(filterx_object_repr(fobj, repr));
  cr_assert_str_eq(test_str, repr->str);
  filterx_object_unref(fobj);
}

Test(filterx_message, test_filterx_message_type_json_repr)
{
  gchar *test_str = "{\"test\":\"json\"}";

  FilterXObject *fobj = filterx_message_value_new(test_str, -1, LM_VT_JSON);
  GString *repr = scratch_buffers_alloc();
  cr_assert(filterx_object_repr(fobj, repr));
  cr_assert_str_eq(test_str, repr->str);
  filterx_object_unref(fobj);
}

Test(filterx_message, test_filterx_message_type_boolean_repr)
{
  gchar *val = "T";
  FilterXObject *fobj = filterx_message_value_new(val, -1, LM_VT_BOOLEAN);
  GString *repr = scratch_buffers_alloc();
  cr_assert(filterx_object_repr(fobj, repr));
  cr_assert_str_eq("true", repr->str);
  filterx_object_unref(fobj);
}

Test(filterx_message, test_filterx_message_type_int_repr)
{
  gchar *val = "443";
  FilterXObject *fobj = filterx_message_value_new(val, -1, LM_VT_INTEGER);
  GString *repr = scratch_buffers_alloc();
  cr_assert(filterx_object_repr(fobj, repr));
  cr_assert_str_eq("443", repr->str);
  filterx_object_unref(fobj);
}

Test(filterx_message, test_filterx_message_type_double_repr)
{
  gchar *val = "17.756";
  FilterXObject *fobj = filterx_message_value_new(val, -1, LM_VT_DOUBLE);
  GString *repr = scratch_buffers_alloc();
  cr_assert(filterx_object_repr(fobj, repr));
  cr_assert_str_eq("17.756", repr->str);
  filterx_object_unref(fobj);
}

Test(filterx_message, test_filterx_message_type_datetime_repr)
{
  gchar *val = "1713520972.000000+02:00";
  FilterXObject *fobj = filterx_message_value_new(val, -1, LM_VT_DATETIME);
  GString *repr = scratch_buffers_alloc();
  cr_assert(filterx_object_repr(fobj, repr));
  cr_assert_str_eq("2024-04-19T12:02:52.000+02:00", repr->str);
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
  scratch_buffers_explicit_gc();
  app_shutdown();
}

TestSuite(filterx_message, .init = setup, .fini = teardown);
