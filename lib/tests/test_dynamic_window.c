/*
 * Copyright (c) 2019 One Identity
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

#include "dynamic-window.h"
#include <criterion/criterion.h>

Test(dynamic_window, window_stat_reset)
{
  DynamicWindow win;
  dynamic_window_stat_reset(&win.stat);
  cr_expect_eq(win.stat.sum, 0);
  cr_expect_eq(win.stat.n, 0);
}

Test(dynamic_window, window_stat_reset_when_counter_is_set)
{
  DynamicWindow win;
  dynamic_window_set_counter(&win, NULL);
  cr_expect_eq(win.stat.sum, 0);
  cr_expect_eq(win.stat.n, 0);
}

Test(dynamic_window, window_stat_avg)
{
  DynamicWindow win;
  dynamic_window_stat_reset(&win.stat);
  dynamic_window_stat_update(&win.stat, 12);
  dynamic_window_stat_update(&win.stat, 4);
  cr_expect_eq(dynamic_window_stat_get_avg(&win.stat), 8);
  cr_expect_eq(dynamic_window_stat_get_number_of_samples(&win.stat), 2);
  cr_expect_eq(dynamic_window_stat_get_sum(&win.stat), 16);
  cr_expect_eq(dynamic_window_stat_get_avg(&win.stat),
               dynamic_window_stat_get_sum(&win.stat) / dynamic_window_stat_get_number_of_samples(&win.stat));
}

