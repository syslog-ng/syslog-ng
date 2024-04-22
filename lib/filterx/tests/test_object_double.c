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
#include "libtest/filterx-lib.h"

#include "filterx/object-string.h"
#include "filterx/object-null.h"
#include "filterx/object-primitive.h"
#include "apphook.h"
#include "scratch-buffers.h"


Test(filterx_double, test_filterx_primitive_double_marshales_to_double_repr)
{
  FilterXObject *fobj = filterx_double_new(36.07);
  assert_marshaled_object(fobj, "36.07", LM_VT_DOUBLE);
  filterx_object_unref(fobj);
}

Test(filterx_double, test_filterx_primitive_double_is_mapped_to_a_json_float)
{
  FilterXObject *fobj = filterx_double_new(36.0);
  assert_object_json_equals(fobj, "36.0");
  filterx_object_unref(fobj);
}

Test(filterx_double, test_filterx_primitive_double_is_truthy_if_nonzero)
{
  FilterXObject *fobj = filterx_double_new(1);
  cr_assert(filterx_object_truthy(fobj));
  cr_assert_not(filterx_object_falsy(fobj));
  filterx_object_unref(fobj);

  fobj = filterx_double_new(0.0);
  cr_assert_not(filterx_object_truthy(fobj));
  cr_assert(filterx_object_falsy(fobj));
  filterx_object_unref(fobj);
}

Test(filterx_double, test_filterx_double_typecast_null_args)
{
  GPtrArray *args = NULL;

  FilterXObject *obj = filterx_typecast_double(args);
  cr_assert_null(obj);
}

Test(filterx_double, test_filterx_double_typecast_empty_args)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);

  FilterXObject *obj = filterx_typecast_double(args);
  cr_assert_null(obj);

  g_ptr_array_free(args, TRUE);
}

Test(filterx_double, test_filterx_double_typecast_null_arg)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);

  g_ptr_array_add(args, NULL);

  FilterXObject *obj = filterx_typecast_double(args);
  cr_assert_null(obj);

  g_ptr_array_free(args, TRUE);
}

Test(filterx_double, test_filterx_double_typecast_null_object_arg)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_null_new();
  g_ptr_array_add(args, in);

  FilterXObject *obj = filterx_typecast_double(args);
  cr_assert_null(obj);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}

Test(filterx_double, test_filterx_double_typecast_from_double)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_double_new(3.14);
  g_ptr_array_add(args, in);

  FilterXObject *obj = filterx_typecast_double(args);
  cr_assert_eq(in, obj);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}

Test(filterx_double, test_filterx_double_typecast_from_integer)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_integer_new(443);
  g_ptr_array_add(args, in);

  FilterXObject *obj = filterx_typecast_double(args);
  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(double)));

  GenericNumber gn = filterx_primitive_get_value(obj);
  cr_assert_float_eq(443.0, gn_as_double(&gn), 0.00001);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}


Test(filterx_double, test_filterx_double_typecast_from_string)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_string_new("443.117", -1);
  g_ptr_array_add(args, in);

  FilterXObject *obj = filterx_typecast_double(args);
  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(double)));

  GenericNumber gn = filterx_primitive_get_value(obj);
  cr_assert_float_eq(443.117, gn_as_double(&gn), 0.00001);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}

Test(filterx_double, test_filterx_double_repr)
{
  FilterXObject *obj = filterx_double_new(123.456);
  GString *repr = scratch_buffers_alloc();
  cr_assert(filterx_object_repr(obj, repr));
  cr_assert_str_eq("123.456", repr->str);
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

TestSuite(filterx_double, .init = setup, .fini = teardown);
