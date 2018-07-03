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

#include "atomic-gssize.h"
#include <criterion/criterion.h>

Test(test_atomic_gssize, set_min_value)
{
  atomic_gssize min;
  atomic_gssize_set(&min, G_MINSSIZE);
  cr_assert_eq(atomic_gssize_get(&min), G_MINSSIZE);
  atomic_gssize_inc(&min);
  cr_assert_eq(atomic_gssize_get(&min), G_MINSSIZE+1);
  atomic_gssize_sub(&min, 2);
  cr_assert_eq(atomic_gssize_get(&min), G_MAXSSIZE);
}

Test(test_atomic_gssize, set_max_value)
{
  atomic_gssize max;
  atomic_gssize_set(&max, G_MAXSSIZE);
  cr_assert_eq(atomic_gssize_get(&max), G_MAXSSIZE);
  atomic_gssize_dec(&max);
  cr_assert_eq(atomic_gssize_get(&max), G_MAXSSIZE - 1);
  gssize old = atomic_gssize_add(&max, 2);
  cr_assert_eq(old, G_MAXSSIZE - 1);
  cr_assert_eq(atomic_gssize_get(&max), G_MINSSIZE);
}

Test(test_atomic_gssize, use_as_unsigned)
{
  atomic_gssize a;
  atomic_gssize_set(&a, G_MAXSIZE);
  cr_assert_eq(atomic_gssize_get_unsigned(&a), G_MAXSIZE);
  atomic_gssize_inc(&a);
  cr_assert_eq(atomic_gssize_get_unsigned(&a), 0);
  atomic_gssize_dec(&a);
  cr_assert_eq(atomic_gssize_get_unsigned(&a), G_MAXSIZE);
}
