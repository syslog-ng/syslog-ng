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

#include <unistd.h>
#include <string.h>
#include <criterion/criterion.h>

#include "secret-storage/nondumpable-allocator.h"

Test(nondumpableallocator, malloc_realloc_free)
{
  char test_string[] = "test_string";
  gpointer buffer = nondumpable_buffer_alloc(strlen(test_string));
  strcpy(buffer, test_string);
  cr_assert_str_eq(buffer, test_string);

  const gsize pagesize = sysconf(_SC_PAGE_SIZE);
  gpointer buffer_realloc = nondumpable_buffer_realloc(buffer, 2*pagesize);
  cr_assert_str_eq(buffer_realloc, test_string);
  ((gchar *)buffer_realloc)[2*pagesize] = 'a';

  nondumpable_buffer_free(buffer_realloc);
}

Test(nondumpableallocator, malloc_mem_zero)
{
  const gsize buffer_len = 256;
  const guchar dummy_value = 0x44;

  guchar *buffer = nondumpable_buffer_alloc(buffer_len);
  memset(buffer, dummy_value, buffer_len);

  for (gsize i = 0; i < buffer_len; ++i)
    cr_assert_eq(buffer[i], dummy_value);

  nondumpable_mem_zero(buffer, buffer_len);

  for (gsize i = 0; i < buffer_len; ++i)
    cr_assert_eq(buffer[i], 0);

  nondumpable_buffer_free(buffer);
}

Test(nondumpableallocator, two_malloc)
{
  char test_string1[] = "test_string2";
  gpointer buffer1 = nondumpable_buffer_alloc(strlen(test_string1));
  strcpy(buffer1, test_string1);

  char test_string2[] = "test_string2";
  gpointer buffer2 = nondumpable_buffer_alloc(strlen(test_string2));
  strcpy(buffer2, test_string2);

  cr_assert_str_eq(buffer1, test_string1);
  cr_assert_str_eq(buffer2, test_string2);

  nondumpable_buffer_free(buffer1);
  nondumpable_buffer_free(buffer2);
}
