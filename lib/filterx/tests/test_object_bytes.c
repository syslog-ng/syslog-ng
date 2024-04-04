/*
 * Copyright (c) 2024 shifter <shifter@axoflow.com>
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
#include "filterx/object-string.h"
#include "filterx/object-null.h"

#include "apphook.h"
#include "scratch-buffers.h"

Test(filterx_bytes, test_filterx_bytes_typecast_null_args)
{
  GPtrArray *args = NULL;

  FilterXObject *obj = filterx_typecast_bytes(args);
  cr_assert_null(obj);
}

Test(filterx_bytes, test_filterx_bytes_typecast_empty_args)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);

  FilterXObject *obj = filterx_typecast_bytes(args);
  cr_assert_null(obj);

  g_ptr_array_free(args, TRUE);
}

Test(filterx_bytes, test_filterx_bytes_typecast_null_arg)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);

  g_ptr_array_add(args, NULL);

  FilterXObject *obj = filterx_typecast_bytes(args);
  cr_assert_null(obj);

  g_ptr_array_free(args, TRUE);
}

Test(filterx_bytes, test_filterx_bytes_typecast_null_object_arg)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_null_new();
  g_ptr_array_add(args, in);

  FilterXObject *obj = filterx_typecast_bytes(args);
  cr_assert_null(obj);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}

Test(filterx_bytes, test_filterx_bytes_typecast_from_bytes)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_bytes_new("byte sequence", -1);
  g_ptr_array_add(args, in);

  FilterXObject *obj = filterx_typecast_bytes(args);

  cr_assert_eq(in, obj);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}

Test(filterx_bytes, test_filterx_bytes_typecast_from_string)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_string_new("string whatever", -1);
  g_ptr_array_add(args, in);

  FilterXObject *obj = filterx_typecast_bytes(args);
  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(bytes)));

  gsize size;
  const gchar *bytes = filterx_bytes_get_value(obj, &size);

  cr_assert(memcmp("string whatever", bytes, size) == 0);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}

Test(filterx_bytes, test_filterx_bytes_typecast_from_protobuf)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_protobuf_new("not a valid protobuf!", -1);
  g_ptr_array_add(args, in);

  FilterXObject *obj = filterx_typecast_bytes(args);
  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(bytes)));

  gsize size;
  const gchar *bytes = filterx_bytes_get_value(obj, &size);

  cr_assert(memcmp("not a valid protobuf!", bytes, size) == 0);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
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

TestSuite(filterx_bytes, .init = setup, .fini = teardown);
