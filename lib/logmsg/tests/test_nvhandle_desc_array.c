/*
 * Copyright (c) 2018 Balabit
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

#include "logmsg/nvhandle-descriptors.h"

#include "string.h"
#include <criterion/criterion.h>

Test(nvhandle_desc_array, add_elements)
{
  NVHandleDesc desc0 = { .name = g_strdup("test0"), .flags = 0, .name_len = strlen("test0")};
  NVHandleDesc desc1 = { .name = g_strdup("test1"), .flags = 1, .name_len = strlen("test1")};
  NVHandleDesc desc2 = { .name = g_strdup("test2"), .flags = 2, .name_len = strlen("test2")};
  NVHandleDesc desc3 = { .name = g_strdup("test3"), .flags = 3, .name_len = strlen("test3")};

  NVHandleDescArray *array = nvhandle_desc_array_new(2);
  nvhandle_desc_array_append(array, &desc0);
  nvhandle_desc_array_append(array, &desc1);
  nvhandle_desc_array_append(array, &desc2);
  nvhandle_desc_array_append(array, &desc3);
  cr_assert_eq(array->len, 4);

  NVHandleDesc *candidate = &nvhandle_desc_array_index(array, 2);
  cr_assert_eq(candidate->flags, 2);
  cr_assert_str_eq(candidate->name, "test2");
  cr_assert_eq(candidate->name_len, strlen("test2"));

  nvhandle_desc_array_free(array);
}
