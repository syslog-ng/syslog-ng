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



Test(filterx_integer, test_filterx_primitive_int_marshales_to_integer_repr)
{
  FilterXObject *fobj = filterx_integer_new(36);
  assert_marshaled_object(fobj, "36", LM_VT_INTEGER);
  filterx_object_unref(fobj);
}

Test(filterx_integer, test_filterx_primitive_int_is_mapped_to_a_json_int)
{
  FilterXObject *fobj = filterx_integer_new(36);
  assert_object_json_equals(fobj, "36");
  filterx_object_unref(fobj);
}

Test(filterx_integer, test_filterx_primitive_int_is_truthy_if_nonzero)
{
  FilterXObject *fobj = filterx_integer_new(36);
  cr_assert(filterx_object_truthy(fobj));
  cr_assert_not(filterx_object_falsy(fobj));
  filterx_object_unref(fobj);

  fobj = filterx_integer_new(0);
  cr_assert_not(filterx_object_truthy(fobj));
  cr_assert(filterx_object_falsy(fobj));
  filterx_object_unref(fobj);
}

Test(filterx_integer, test_filterx_integer_typecast_null_args)
{
  GPtrArray *args = NULL;

  FilterXObject *obj = filterx_typecast_integer(args);
  cr_assert_null(obj);
}

Test(filterx_integer, test_filterx_integer_typecast_empty_args)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);

  FilterXObject *obj = filterx_typecast_integer(args);
  cr_assert_null(obj);

  g_ptr_array_free(args, TRUE);
}

Test(filterx_integer, test_filterx_integer_typecast_null_arg)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);

  g_ptr_array_add(args, NULL);

  FilterXObject *obj = filterx_typecast_integer(args);
  cr_assert_null(obj);

  g_ptr_array_free(args, TRUE);
}

// null->int cast can might be result in 0 later
Test(filterx_integer, test_filterx_integer_typecast_null_object_arg)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_null_new();
  g_ptr_array_add(args, in);

  FilterXObject *obj = filterx_typecast_integer(args);
  cr_assert_null(obj);

  g_ptr_array_free(args, TRUE);
}

Test(filterx_integer, test_filterx_integer_typecast_from_integer)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_integer_new(443);
  g_ptr_array_add(args, in);

  FilterXObject *obj = filterx_typecast_integer(args);
  cr_assert_eq(in, obj);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}

Test(filterx_integer, test_filterx_integer_typecast_from_double)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_double_new(171.443);
  g_ptr_array_add(args, in);

  FilterXObject *obj = filterx_typecast_integer(args);
  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(integer)));

  GenericNumber gn = filterx_primitive_get_value(obj);

  cr_assert(gn_as_int64(&gn) == 171);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}

// generic number double->int conversion now uses round, we use it as a first attempt
// ceil/floor helper functions or exact int(double) typecasts are possible solutions later
Test(filterx_integer, test_filterx_integer_typecast_from_double_roundup)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_double_new(171.743);
  g_ptr_array_add(args, in);

  FilterXObject *obj = filterx_typecast_integer(args);
  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(integer)));

  GenericNumber gn = filterx_primitive_get_value(obj);

  cr_assert(gn_as_int64(&gn) == 172);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}

Test(filterx_integer, test_filterx_integer_typecast_from_string)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_string_new("443", -1);
  g_ptr_array_add(args, in);

  FilterXObject *obj = filterx_typecast_integer(args);
  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(integer)));

  GenericNumber gn = filterx_primitive_get_value(obj);

  cr_assert(gn_as_int64(&gn) == 443);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}

Test(filterx_integer, test_filterx_integer_typecast_from_string_very_very_zero)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_string_new("000", -1);
  g_ptr_array_add(args, in);

  FilterXObject *obj = filterx_typecast_integer(args);
  cr_assert_not_null(obj);
  cr_assert(filterx_object_is_type(obj, &FILTERX_TYPE_NAME(integer)));

  GenericNumber gn = filterx_primitive_get_value(obj);

  cr_assert(gn_as_int64(&gn) == 0);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}

Test(filterx_integer, test_filterx_integer_typecast_from_double_string)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *in = filterx_string_new("443.117", -1);
  g_ptr_array_add(args, in);

  FilterXObject *obj = filterx_typecast_integer(args);
  cr_assert_null(obj);

  g_ptr_array_free(args, TRUE);
  filterx_object_unref(obj);
}

Test(filterx_integer, test_filterx_integer_repr)
{
  FilterXObject *obj = filterx_integer_new(65566);
  GString *repr = scratch_buffers_alloc();
  cr_assert(filterx_object_repr(obj, repr));
  cr_assert_str_eq("65566", repr->str);
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

TestSuite(filterx_integer, .init = setup, .fini = teardown);
