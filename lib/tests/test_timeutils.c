/*
 * Copyright (c) 2017 Balabit
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
#include <criterion/theories.h>
#include <time.h>

#include "timeutils/timeutils.h"

TheoryDataPoints(msgparse, test_year_calculation_for_bsd_format_regular_months) =
{
  DataPoints(int, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11),
  DataPoints(int, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11),
};

Theory((int lhs, int rhs), msgparse, test_year_calculation_for_bsd_format_regular_months)
{
  /* For regular months, the year will be the actual year, independently of the current month. */
  cr_assume(lhs != 11 || rhs != 0);
  cr_assume(lhs != 0 || rhs != 11);

  struct tm *tm;
  time_t t;

  time(&t);
  tm = localtime(&t);
  tm->tm_mon = rhs;
  cr_assert_eq(tm->tm_year, determine_year_for_month(lhs, tm));
}

Test(msgparse, test_bsd_year_calculation_near_to_end_of_the_year)
{
  struct tm *tm;
  time_t t;

  time(&t);
  tm = localtime(&t);

  /* Current month is January, the given month is December => last year */
  tm->tm_mon = 0;
  cr_assert_eq(tm->tm_year - 1, determine_year_for_month(11, tm));

  /* Current month is December, the given month is January => next year */
  tm->tm_mon = 11;
  cr_assert_eq(tm->tm_year + 1, determine_year_for_month(0, tm));
}
