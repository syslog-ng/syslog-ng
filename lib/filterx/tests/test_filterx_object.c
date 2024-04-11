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
#include "libtest/cr_template.h"

#include "filterx/filterx-object.h"
#include "filterx/filterx-globals.h"
#include "filterx/object-string.h"
#include "filterx/object-primitive.h"

#include "apphook.h"

FILTERX_DECLARE_TYPE(dummy_base);
FILTERX_DECLARE_TYPE(dummy);
FILTERX_DEFINE_TYPE(dummy_base, FILTERX_TYPE_NAME(object));
FILTERX_DEFINE_TYPE(dummy, FILTERX_TYPE_NAME(dummy_base));

Test(filterx_object, test_filterx_object_is_type_builtin_null_args)
{
  FilterXObject *out = filterx_object_is_type_builtin(NULL);
  cr_assert_null(out);
}

Test(filterx_object, test_filterx_object_is_type_builtin_empty_args)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  FilterXObject *out = filterx_object_is_type_builtin(args);
  cr_assert_null(out);
  g_ptr_array_free(args, TRUE);
}

Test(filterx_object, test_filterx_object_is_type_builtin_null_object_arg)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  g_ptr_array_add(args, NULL);
  g_ptr_array_add(args, filterx_string_new("json_object", -1));
  FilterXObject *out = filterx_object_is_type_builtin(args);
  cr_assert_null(out);
  g_ptr_array_free(args, TRUE);
}

Test(filterx_object, test_filterx_object_is_type_builtin_null_type_arg)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);

  g_ptr_array_add(args, filterx_object_new(&FILTERX_TYPE_NAME(dummy)));
  g_ptr_array_add(args, NULL);
  FilterXObject *out = filterx_object_is_type_builtin(args);
  cr_assert_null(out);
  g_ptr_array_free(args, TRUE);
}

Test(filterx_object, test_filterx_object_is_type_builtin_invalid_type_arg)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  g_ptr_array_add(args, filterx_object_new(&FILTERX_TYPE_NAME(dummy)));
  g_ptr_array_add(args, filterx_integer_new(33));
  FilterXObject *out = filterx_object_is_type_builtin(args);
  cr_assert_null(out);
  g_ptr_array_free(args, TRUE);
}

Test(filterx_object, test_filterx_object_is_type_builtin_too_many_args)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  g_ptr_array_add(args, filterx_object_new(&FILTERX_TYPE_NAME(dummy)));
  g_ptr_array_add(args, filterx_string_new("dummy", -1));
  g_ptr_array_add(args, filterx_string_new("dummy_base", -1));
  FilterXObject *out = filterx_object_is_type_builtin(args);
  cr_assert_null(out);
  g_ptr_array_free(args, TRUE);
}

Test(filterx_object, test_filterx_object_is_type_builtin_non_matching_type)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  g_ptr_array_add(args, filterx_object_new(&FILTERX_TYPE_NAME(dummy)));
  g_ptr_array_add(args, filterx_string_new("string", -1));
  FilterXObject *out = filterx_object_is_type_builtin(args);
  cr_assert_not_null(out);
  cr_assert(filterx_object_is_type(out, &FILTERX_TYPE_NAME(boolean)));
  cr_assert(filterx_object_falsy(out));
  g_ptr_array_free(args, TRUE);
  filterx_object_unref(out);
}

Test(filterx_object, test_filterx_object_is_type_builtin_matching_type)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  g_ptr_array_add(args, filterx_object_new(&FILTERX_TYPE_NAME(dummy)));
  g_ptr_array_add(args, filterx_string_new("dummy", -1));
  FilterXObject *out = filterx_object_is_type_builtin(args);
  cr_assert_not_null(out);
  cr_assert(filterx_object_is_type(out, &FILTERX_TYPE_NAME(boolean)));
  cr_assert(filterx_object_truthy(out));
  g_ptr_array_free(args, TRUE);
  filterx_object_unref(out);
}

Test(filterx_object, test_filterx_object_is_type_builtin_matching_type_for_super_type)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  g_ptr_array_add(args, filterx_object_new(&FILTERX_TYPE_NAME(dummy)));
  g_ptr_array_add(args, filterx_string_new("dummy_base", -1));
  FilterXObject *out = filterx_object_is_type_builtin(args);
  cr_assert_not_null(out);
  cr_assert(filterx_object_is_type(out, &FILTERX_TYPE_NAME(boolean)));
  cr_assert(filterx_object_truthy(out));
  g_ptr_array_free(args, TRUE);
  filterx_object_unref(out);
}

Test(filterx_object, test_filterx_object_is_type_builtin_matching_type_for_root_type)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  g_ptr_array_add(args, filterx_object_new(&FILTERX_TYPE_NAME(dummy)));
  g_ptr_array_add(args, filterx_string_new("object", -1));
  FilterXObject *out = filterx_object_is_type_builtin(args);
  cr_assert_not_null(out);
  cr_assert(filterx_object_is_type(out, &FILTERX_TYPE_NAME(boolean)));
  cr_assert(filterx_object_truthy(out));
  g_ptr_array_free(args, TRUE);
  filterx_object_unref(out);
}

static void
setup(void)
{
  app_startup();
  filterx_type_init(&FILTERX_TYPE_NAME(dummy_base));
  filterx_type_init(&FILTERX_TYPE_NAME(dummy));
}

static void
teardown(void)
{
  app_shutdown();
}

TestSuite(filterx_object, .init = setup, .fini = teardown);
