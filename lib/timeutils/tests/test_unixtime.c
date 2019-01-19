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
#include <criterion/criterion.h>
#include "fake-time.h"

static void
_wct_initialize(WallClockTime *wct, const gchar *timestamp)
{
  gchar *end = wall_clock_time_strptime(wct, "%b %d %Y %H:%M:%S", timestamp);

  cr_assert(*end == 0, "error parsing WallClockTime initialization timestamp: %s, end: %s", timestamp, end);
}

static void
_wct_initialize_with_tz(WallClockTime *wct, const gchar *timestamp)
{
  gchar *end = wall_clock_time_strptime(wct, "%b %d %Y %H:%M:%S %z", timestamp);

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

Test(unixtime, unix_time_set_from_wall_clock_time)
{
  UnixTime ut = UNIX_TIME_INIT;
  WallClockTime wct = WALL_CLOCK_TIME_INIT;

  _wct_initialize(&wct, "Jan 19 2019 18:58:48");
  wct.wct_gmtoff = 3600;

  unix_time_set_from_wall_clock_time(&ut, &wct);
  cr_expect(ut.ut_gmtoff == 3600);
  cr_expect(ut.ut_sec == 1547920728);
}

Test(unixtime, unix_time_set_from_wall_clock_time_without_timezone_assumes_local_tz)
{
  UnixTime ut = UNIX_TIME_INIT;
  WallClockTime wct = WALL_CLOCK_TIME_INIT;

  _wct_initialize(&wct, "Jan 19 2019 18:58:48");
  cr_expect(wct.wct_gmtoff == -1);

  unix_time_set_from_wall_clock_time(&ut, &wct);
  cr_expect(ut.ut_gmtoff == 3600);
  cr_expect(ut.ut_sec == 1547920728);
}

Test(unixtime, unix_time_set_from_normalized_wall_clock_time_changes_the_wct_to_normalize_values)
{
  UnixTime ut = UNIX_TIME_INIT;
  WallClockTime wct = WALL_CLOCK_TIME_INIT;

  _wct_initialize(&wct, "Mar 31 2019 02:11:00");

  /* pre normalized values, just as we parsed it from the input */
  cr_expect(wct.wct_gmtoff == -1);
  cr_expect(wct.wct_hour == 2);

  unix_time_set_from_normalized_wall_clock_time(&ut, &wct);

  /* at this point normalization changes the wct as well! */
  cr_expect(wct.wct_gmtoff == 7200);
  cr_expect(wct.wct_hour == 2);

  cr_expect(ut.ut_gmtoff == 7200);
  cr_expect(ut.ut_sec == 1553994660 - 3600);
}

Test(unixtime, unix_time_set_from_a_specific_timezone_which_happens_at_the_spring_transition_hour)
{
  UnixTime ut = UNIX_TIME_INIT;
  WallClockTime wct = WALL_CLOCK_TIME_INIT;

  /* this timestamp is in the DST transition hour but is from a timezone
   * where the daylight saving happens at another time (Mar 10th) */

  _wct_initialize_with_tz(&wct, "Mar 31 2019 02:11:00 EDT");

  /* pre normalized values, just as we parsed it from the input */
  cr_expect(wct.wct_gmtoff == -5*3600 + 3600);
  cr_expect(wct.wct_isdst == 1);
  cr_expect(wct.wct_hour == 2);

  unix_time_set_from_normalized_wall_clock_time(&ut, &wct);

  /* at this point normalization (e.g.  the stuff mktime() does) might
   * change the wct as well, but
   * unix_time_set_from_normalized_wall_clock_time() tries to behave as if
   * they didn't happen.  */

  cr_expect(wct.wct_gmtoff == -5*3600 + 3600);
  cr_expect(wct.wct_hour == 2);

  cr_expect(ut.ut_gmtoff == -5*3600 + 3600);
  cr_expect(ut.ut_sec == 1554012660);

  /* going back from UnixTime to WallClockTime, we should get the same
   * gmtoff and hour values */

  wall_clock_time_set_from_unix_time(&wct, &ut);
  cr_expect(wct.wct_gmtoff == -5*3600 + 3600);
  cr_expect(wct.wct_hour == 2);
  cr_expect(wct.wct_min == 11);
}

Test(unixtime, unix_time_set_from_a_specific_timezone_which_happens_at_the_autumn_transition_hour)
{
  UnixTime ut = UNIX_TIME_INIT;
  WallClockTime wct = WALL_CLOCK_TIME_INIT;

  /* this timestamp is in the DST transition hour but is from a timezone
   * where the daylight saving happens at another time (Nov 3rd) */

  _wct_initialize_with_tz(&wct, "Oct 27 2019 02:11:00 EDT");

  /* pre normalized values, just as we parsed it from the input */
  cr_expect(wct.wct_gmtoff == -5*3600 + 3600);
  cr_expect(wct.wct_isdst == 1);
  cr_expect(wct.wct_hour == 2);

  unix_time_set_from_normalized_wall_clock_time(&ut, &wct);

  /* at this point normalization (e.g.  the stuff mktime() does) might
   * change the wct as well, but
   * unix_time_set_from_normalized_wall_clock_time() tries to behave as if
   * they didn't happen.  */

  cr_expect(wct.wct_gmtoff == -5*3600 + 3600);
  cr_expect(wct.wct_hour == 2);

  cr_expect(ut.ut_gmtoff == -5*3600 + 3600);
  cr_expect(ut.ut_sec == 1572156660);

  /* going back from UnixTime to WallClockTime, we should get the same
   * gmtoff and hour values */

  wall_clock_time_set_from_unix_time(&wct, &ut);
  cr_expect(wct.wct_gmtoff == -5*3600 + 3600);
  cr_expect(wct.wct_hour == 2);
  cr_expect(wct.wct_min == 11);
}

Test(unixtime, unix_time_set_from_wall_clock_time_without_timezone_and_tz_hint_uses_the_hint)
{
  UnixTime ut = UNIX_TIME_INIT;
  WallClockTime wct = WALL_CLOCK_TIME_INIT;

  _wct_initialize(&wct, "Jan 19 2019 18:58:48");
  cr_expect(wct.wct_gmtoff == -1);

  unix_time_set_from_wall_clock_time_with_tz_hint(&ut, &wct, 7200);
  cr_expect(wct.wct_gmtoff == -1);
  cr_expect(ut.ut_gmtoff == 7200);
  cr_expect(ut.ut_sec == 1547917128);

  wct.wct_gmtoff = -1;
  unix_time_set_from_wall_clock_time_with_tz_hint(&ut, &wct, -5*3600);
  cr_expect(wct.wct_gmtoff == -1);
  cr_expect(ut.ut_gmtoff == -5*3600);
  cr_expect(ut.ut_sec == 1547942328);
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
