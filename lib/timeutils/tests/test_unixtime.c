/*
 * Copyright (c) 2002-2019 Balabit
 * Copyright (c) 1998-2019 Balázs Scheidler
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
#include "timeutils/unixtime.h"
#include "timeutils/wallclocktime.h"
#include "timeutils/conv.h"
#include <criterion/criterion.h>
#include "fake-time.h"

static void
_wct_initialize(WallClockTime *wct, const gchar *timestamp)
{
  gchar *end = wall_clock_time_strptime(wct, "%b %d %Y %H:%M:%S", timestamp);

  cr_assert(*end == 0, "error parsing WallClockTime initialization timestamp: %s, end: %s", timestamp, end);
}

Test(unixtime, unix_time_initialization)
{
  UnixTime ut = UNIX_TIME_INIT;

  cr_assert(!unix_time_is_set(&ut));

  /* Thu Dec 19 22:25:44 CET 2019 */
  fake_time(1576790744);
  unix_time_set_now(&ut);

  cr_assert(unix_time_is_set(&ut));
  cr_expect(ut.ut_sec == 1576790744);
  cr_expect(ut.ut_usec == 123000);
  cr_expect(ut.ut_gmtoff == 3600);

  unix_time_unset(&ut);
  cr_assert(!unix_time_is_set(&ut));
}

Test(unixtime, unix_time_fix_timezone_adjusts_timestamp_as_if_was_parsed_assuming_the_incorrect_timezone)
{
  UnixTime ut = UNIX_TIME_INIT;
  WallClockTime wct = WALL_CLOCK_TIME_INIT;

  _wct_initialize(&wct, "Jan 19 2019 18:58:48");
  wct.wct_gmtoff = 3600;
  convert_wall_clock_time_to_unix_time(&wct, &ut);
  cr_expect(ut.ut_gmtoff == 3600);

  unix_time_fix_timezone(&ut, -5*3600);
  cr_expect(ut.ut_gmtoff == -5*3600);
  convert_unix_time_to_wall_clock_time(&ut, &wct);
  cr_expect(wct.wct_hour == 18);
  cr_expect(wct.wct_min == 58);
  cr_expect(wct.wct_sec == 48);
}

Test(unixtime, unix_time_set_timezone_converts_the_timestamp_to_a_target_timezone_assuming_the_source_was_correct)
{
  UnixTime ut = UNIX_TIME_INIT;
  WallClockTime wct = WALL_CLOCK_TIME_INIT;

  _wct_initialize(&wct, "Jan 19 2019 18:58:48");
  wct.wct_gmtoff = 3600;
  convert_wall_clock_time_to_unix_time(&wct, &ut);
  cr_expect(ut.ut_gmtoff == 3600);

  unix_time_set_timezone(&ut, -5*3600);
  cr_expect(ut.ut_gmtoff == -5*3600);
  convert_unix_time_to_wall_clock_time(&ut, &wct);
  cr_expect(wct.wct_hour == 12);
  cr_expect(wct.wct_min == 58);
  cr_expect(wct.wct_sec == 48);
}

static void
setup(void)
{
  setenv("TZ", "CET", TRUE);
  tzset();
}

static void
teardown(void)
{
}

TestSuite(unixtime, .init = setup, .fini = teardown);
