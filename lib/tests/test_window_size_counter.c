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

#include "syslog-ng.h"
#include "window-size-counter.h"
#include <criterion/criterion.h>


Test(test_window_size_counter, suspend_resume)
{
  WindowSizeCounter c;
  gboolean suspended = FALSE;
  window_size_counter_set(&c, 10);
  cr_expect_not(window_size_counter_suspended(&c));

  window_size_counter_sub(&c, 10, &suspended);
  cr_expect_not(suspended);
  cr_expect(window_size_counter_suspended(&c));

  window_size_counter_add(&c, 10, &suspended);
  cr_expect(suspended);
  cr_expect_not(window_size_counter_suspended(&c));

  window_size_counter_suspend(&c);
  cr_expect(window_size_counter_suspended(&c));

  gsize val = window_size_counter_get(&c, &suspended);
  cr_expect(suspended);
  cr_expect_eq(val, 10);

  window_size_counter_add(&c, 1, &suspended);
  cr_expect_eq(window_size_counter_get(&c, &suspended), 11);
  window_size_counter_resume(&c);
  cr_expect_not(window_size_counter_suspended(&c));
}

Test(test_window_size_counter, negative_value)
{
  WindowSizeCounter c;
  gboolean suspended = FALSE;
  window_size_counter_set(&c, -1);
  gint v = (gint)window_size_counter_get(&c, &suspended);
  cr_assert_eq(v, -1);
}

Test(test_window_size_counter, suspend_resume_multiple_times)
{
  WindowSizeCounter c;
  window_size_counter_set(&c, window_size_counter_get_max());

  window_size_counter_resume(&c);
  cr_expect_not(window_size_counter_suspended(&c));
  gboolean suspended;
  cr_expect_eq(window_size_counter_get(&c, &suspended), window_size_counter_get_max());
  cr_expect_not(suspended);
  window_size_counter_resume(&c);
  cr_expect_not(window_size_counter_suspended(&c));
  cr_expect_eq(window_size_counter_get(&c, &suspended), window_size_counter_get_max());
  cr_expect_not(suspended);

  window_size_counter_suspend(&c);
  cr_expect(window_size_counter_suspended(&c));
  cr_expect_eq(window_size_counter_get(&c, &suspended), window_size_counter_get_max());
  cr_expect(suspended);

  window_size_counter_suspend(&c);
  cr_expect(window_size_counter_suspended(&c));
  cr_expect_eq(window_size_counter_get(&c, &suspended), window_size_counter_get_max());
  cr_expect(suspended);
  window_size_counter_resume(&c);
  cr_expect_not(window_size_counter_suspended(&c));
  cr_expect_eq(window_size_counter_get(&c, &suspended), window_size_counter_get_max());
  cr_expect_not(suspended);
}
