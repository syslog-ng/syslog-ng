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

#include "filterx/object-null.h"
#include "apphook.h"
#include "scratch-buffers.h"

Test(filterx_null, test_filterx_object_null_marshals_to_the_stored_values)
{
  FilterXObject *fobj = filterx_null_new();
  assert_marshaled_object(fobj, "", LM_VT_NULL);
  filterx_object_unref(fobj);
}

Test(filterx_null, test_filterx_object_null_maps_to_the_right_json_value)
{
  FilterXObject *fobj = filterx_null_new();
  assert_object_json_equals(fobj, "null");
  filterx_object_unref(fobj);
}

Test(filterx_null, test_filterx_object_null_repr)
{
  FilterXObject *fobj = filterx_null_new();
  GString *repr = scratch_buffers_alloc();
  cr_assert(filterx_object_repr(fobj, repr));
  cr_assert_str_eq("null", repr->str);
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

TestSuite(filterx_null, .init = setup, .fini = teardown);
