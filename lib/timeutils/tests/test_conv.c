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

static void
_wct_initialize_with_tz(WallClockTime *wct, const gchar *timestamp)
{
  gchar *end = wall_clock_time_strptime(wct, "%b %d %Y %H:%M:%S %z", timestamp);

  cr_assert(*end == 0, "error parsing WallClockTime initialization timestamp: %s, end: %s", timestamp, end);
}

Test(conv, convert_wall_clock_time_to_unix_time)
{
  UnixTime ut = UNIX_TIME_INIT;
  WallClockTime wct = WALL_CLOCK_TIME_INIT;

  _wct_initialize(&wct, "Jan 19 2019 18:58:48");
  wct.wct_gmtoff = 3600;

  convert_wall_clock_time_to_unix_time(&wct, &ut);
  cr_expect(ut.ut_gmtoff == 3600);
  cr_expect(ut.ut_sec == 1547920728);
}

Test(conv, convert_wall_clock_time_to_unix_time_without_timezone_assumes_local_tz)
{
  UnixTime ut = UNIX_TIME_INIT;
  WallClockTime wct = WALL_CLOCK_TIME_INIT;

  _wct_initialize(&wct, "Jan 19 2019 18:58:48");
  cr_expect(wct.wct_gmtoff == -1);

  convert_wall_clock_time_to_unix_time(&wct, &ut);
  cr_expect(ut.ut_gmtoff == 3600);
  cr_expect(ut.ut_sec == 1547920728);
}

Test(conv, convert_and_normalize_wall_clock_time_to_unix_time_changes_the_wct_to_normalize_values)
{
  UnixTime ut = UNIX_TIME_INIT;
  WallClockTime wct = WALL_CLOCK_TIME_INIT;

  _wct_initialize(&wct, "Mar 31 2019 02:11:00");

  /* pre normalized values, just as we parsed it from the input */
  cr_expect(wct.wct_gmtoff == -1);
  cr_expect(wct.wct_hour == 2);

  convert_and_normalize_wall_clock_time_to_unix_time(&wct, &ut);

  /* at this point normalization changes the wct as well! */
  cr_expect(wct.wct_gmtoff == 7200);
  cr_expect(wct.wct_hour == 2);

  cr_expect(ut.ut_gmtoff == 7200);
  cr_expect(ut.ut_sec == 1553994660 - 3600);
}

Test(conv, unix_time_set_from_a_specific_timezone_which_happens_at_the_spring_transition_hour)
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

  convert_and_normalize_wall_clock_time_to_unix_time(&wct, &ut);

  /* at this point normalization (e.g.  the stuff mktime() does) might
   * change the wct as well, but
   * convert_and_normalize_wall_clock_time_to_unix_time() tries to behave as if
   * they didn't happen.  */

  cr_expect(wct.wct_gmtoff == -5*3600 + 3600);
  cr_expect(wct.wct_hour == 2);

  cr_expect(ut.ut_gmtoff == -5*3600 + 3600);
  cr_expect(ut.ut_sec == 1554012660);

  /* going back from UnixTime to WallClockTime, we should get the same
   * gmtoff and hour values */

  convert_unix_time_to_wall_clock_time(&ut, &wct);
  cr_expect(wct.wct_gmtoff == -5*3600 + 3600);
  cr_expect(wct.wct_hour == 2);
  cr_expect(wct.wct_min == 11);
}

Test(conv, unix_time_set_from_a_specific_timezone_which_happens_at_the_autumn_transition_hour)
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

  convert_and_normalize_wall_clock_time_to_unix_time(&wct, &ut);

  /* at this point normalization (e.g.  the stuff mktime() does) might
   * change the wct as well, but
   * convert_and_normalize_wall_clock_time_to_unix_time() tries to behave as if
   * they didn't happen.  */

  cr_expect(wct.wct_gmtoff == -5*3600 + 3600);
  cr_expect(wct.wct_hour == 2);

  cr_expect(ut.ut_gmtoff == -5*3600 + 3600);
  cr_expect(ut.ut_sec == 1572156660);

  /* going back from UnixTime to WallClockTime, we should get the same
   * gmtoff and hour values */

  convert_unix_time_to_wall_clock_time(&ut, &wct);
  cr_expect(wct.wct_gmtoff == -5*3600 + 3600);
  cr_expect(wct.wct_hour == 2);
  cr_expect(wct.wct_min == 11);
}

Test(conv, convert_wall_clock_time_to_unix_time_without_timezone_and_tz_hint_uses_the_hint)
{
  UnixTime ut = UNIX_TIME_INIT;
  WallClockTime wct = WALL_CLOCK_TIME_INIT;

  _wct_initialize(&wct, "Jan 19 2019 18:58:48");
  cr_expect(wct.wct_gmtoff == -1);

  convert_wall_clock_time_to_unix_time_with_tz_hint(&wct, &ut, 7200);
  cr_expect(wct.wct_gmtoff == -1);
  cr_expect(ut.ut_gmtoff == 7200);
  cr_expect(ut.ut_sec == 1547917128);

  wct.wct_gmtoff = -1;
  convert_wall_clock_time_to_unix_time_with_tz_hint(&wct, &ut, -5*3600);
  cr_expect(wct.wct_gmtoff == -1);
  cr_expect(ut.ut_gmtoff == -5*3600);
  cr_expect(ut.ut_sec == 1547942328);
}

Test(conv, set_from_unixtime_sets_wct_fields_properly)
{
  WallClockTime wct = WALL_CLOCK_TIME_INIT;
  UnixTime ut = UNIX_TIME_INIT;

  /* Thu Dec 19 22:25:44 CET 2019 */
  ut.ut_sec = 1576790744;
  ut.ut_usec = 567000;
  ut.ut_gmtoff = 3600;

  convert_unix_time_to_wall_clock_time(&ut, &wct);
  cr_expect(wct.wct_year == 119);
  cr_expect(wct.wct_mon == 11);
  cr_expect(wct.wct_mday == 19);

  cr_expect(wct.wct_hour == 22);
  cr_expect(wct.wct_min == 25);
  cr_expect(wct.wct_sec == 44);
  cr_expect(wct.wct_usec == 567000);
  cr_expect(wct.wct_gmtoff == 3600);
}

Test(conv, set_from_unixtime_with_a_different_gmtoff_changes_hours_properly)
{
  WallClockTime wct = WALL_CLOCK_TIME_INIT;
  UnixTime ut = UNIX_TIME_INIT;

  /* Thu Dec 19 16:25:44 EST 2019 */
  ut.ut_sec = 1576790744;
  ut.ut_usec = 567000;
  ut.ut_gmtoff = -5*3600;

  convert_unix_time_to_wall_clock_time(&ut, &wct);
  cr_expect(wct.wct_year == 119);
  cr_expect(wct.wct_mon == 11);
  cr_expect(wct.wct_mday == 19);

  cr_expect(wct.wct_hour == 16);
  cr_expect(wct.wct_min == 25);
  cr_expect(wct.wct_sec == 44);
  cr_expect(wct.wct_usec == 567000);
  cr_expect(wct.wct_gmtoff == -5*3600);
}

Test(conv, set_from_unixtime_without_timezone_information_assumes_local_timezone)
{
  WallClockTime wct = WALL_CLOCK_TIME_INIT;
  UnixTime ut = UNIX_TIME_INIT;

  /* Thu Dec 19 22:25:44 CET 2019 */
  ut.ut_sec = 1576790744;
  ut.ut_usec = 567000;
  ut.ut_gmtoff = -1;

  convert_unix_time_to_wall_clock_time(&ut, &wct);
  cr_expect(wct.wct_year == 119);
  cr_expect(wct.wct_mon == 11);
  cr_expect(wct.wct_mday == 19);

  cr_expect(wct.wct_hour == 22);
  cr_expect(wct.wct_min == 25);
  cr_expect(wct.wct_sec == 44);
  cr_expect(wct.wct_usec == 567000);
  cr_expect(wct.wct_gmtoff == 3600);
}

Test(conv, set_from_unixtime_with_tz_override_changes_the_timezone_to_the_overridden_value)
{
  WallClockTime wct = WALL_CLOCK_TIME_INIT;
  UnixTime ut = UNIX_TIME_INIT;

  /* Thu Dec 19 22:25:44 CET 2019 */
  ut.ut_sec = 1576790744;
  ut.ut_usec = 567000;
  ut.ut_gmtoff = 3600;

  /* +05:30 */
  convert_unix_time_to_wall_clock_time_with_tz_override(&ut, &wct, 5*3600 + 1800);
  cr_expect(wct.wct_year == 119);
  cr_expect(wct.wct_mon == 11);
  cr_expect(wct.wct_mday == 20);

  cr_expect(wct.wct_hour == 2);
  cr_expect(wct.wct_min == 55);
  cr_expect(wct.wct_sec == 44);
  cr_expect(wct.wct_usec == 567000);
  cr_expect(wct.wct_gmtoff == 5*3600 + 1800);
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

TestSuite(conv, .init = setup, .fini = teardown);
