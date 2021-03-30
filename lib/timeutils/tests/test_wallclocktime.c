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
#include <criterion/criterion.h>
#include "timeutils/wallclocktime.h"
#include "timeutils/cache.h"
#include "timeutils/conv.h"
#include "fake-time.h"

Test(wallclocktime, test_wall_clock_time_init)
{
  WallClockTime wct;

  wall_clock_time_unset(&wct);
  cr_assert(wall_clock_time_is_set(&wct) == FALSE);
}

Test(wallclocktime, test_strptime_parses_broken_down_time)
{
  WallClockTime wct = WALL_CLOCK_TIME_INIT;
  gchar *end;

  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S.%f", "Jan 16 2019 18:23:12.012345");
  cr_assert(*end == 0);
  cr_expect(wct.wct_year == 119);
  cr_expect(wct.wct_mon == 0);
  cr_expect(wct.wct_mday == 16);

  cr_expect(wct.wct_hour == 18);
  cr_expect(wct.wct_min == 23);
  cr_expect(wct.wct_sec == 12);
  cr_expect(wct.wct_usec == 12345);

  cr_expect(wct.wct_gmtoff == -1);
}

Test(wallclocktime, test_strptime_parses_truncated_usec)
{
  WallClockTime wct = WALL_CLOCK_TIME_INIT;
  gchar *end;

  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S.%f", "Jan 16 2019 18:23:12.012X");
  cr_assert(*end == 'X');
  cr_expect(wct.wct_usec == 12000);
}

Test(wallclocktime, test_strptime_parses_overflowed_usec)
{
  WallClockTime wct = WALL_CLOCK_TIME_INIT;
  gchar *end;

  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S.%f", "Jan 16 2019 18:23:12.0123456X");
  cr_assert(*end == 'X');
  cr_expect(wct.wct_usec == 12345);
}

Test(wallclocktime, test_strptime_usec_parse_finds_character)
{
  WallClockTime wct = WALL_CLOCK_TIME_INIT;
  gchar *end;

  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S.%f", "Jan 16 2019 18:23:12.boom");
  cr_assert(end == NULL);
  cr_expect(wct.wct_usec == 0);
}

Test(wallclocktime, test_strptime_percent_z_parses_rfc822_timezone)
{
  WallClockTime wct = WALL_CLOCK_TIME_INIT;
  gchar *end;

  /* quoting RFC822:
   *
   * Time zone may be indicated in several ways.  "UT" is Universal Time
   * (formerly called "Greenwich Mean Time"); "GMT" is permitted as a
   * reference to Universal Time.  The military standard uses a single
   * character for each zone.  "Z" is Universal Time.  "A" indicates one
   * hour earlier, and "M" indicates 12 hours earlier; "N" is one hour
   * later, and "Y" is 12 hours later.  The letter "J" is not used.  The
   * other remaining two forms are taken from ANSI standard X3.51-1975.  One
   * allows explicit indication of the amount of offset from UT; the other
   * uses common 3-character strings for indicating time zones in North
   * America.
   */

  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S %z", "Jan 16 2019 18:23:12 PST");
  cr_assert(*end == 0);
  cr_expect(wct.wct_year == 119);
  cr_expect(wct.wct_mon == 0);
  cr_expect(wct.wct_mday == 16);

  cr_expect(wct.wct_hour == 18);
  cr_expect(wct.wct_min == 23);
  cr_expect(wct.wct_sec == 12);
  cr_expect(wct.wct_usec == 0);

  cr_expect(wct.wct_gmtoff == -8*3600, "Unexpected timezone offset: %ld, expected -8*3600", wct.wct_gmtoff);

  /* white space in front of the timezone is skipped with %z */
  wct.wct_gmtoff = -1;
  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S%z", "Jan 16 2019 18:23:12 PST");
  cr_expect(wct.wct_gmtoff == -8*3600, "Unexpected timezone offset: %ld, expected -8*3600", wct.wct_gmtoff);
  wct.wct_gmtoff = -1;
  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S%z", "Jan 16 2019 18:23:12PST");
  cr_expect(wct.wct_gmtoff == -8*3600, "Unexpected timezone offset: %ld, expected -8*3600", wct.wct_gmtoff);

  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S %z", "Jan 16 2019 18:23:12 EDT");
  cr_expect(wct.wct_gmtoff == -4*3600, "Unexpected timezone offset: %ld, expected -4*3600", wct.wct_gmtoff);

  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S %z", "Jan 16 2019 18:23:12 GMT");
  cr_expect(wct.wct_gmtoff == 0, "Unexpected timezone offset: %ld, expected 0", wct.wct_gmtoff);

  wct.wct_gmtoff = -1;
  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S %z", "Jan 16 2019 18:23:12 UTC");
  cr_expect(wct.wct_gmtoff == 0, "Unexpected timezone offset: %ld, expected 0", wct.wct_gmtoff);

  /* local timezone */
  wct.wct_gmtoff = -1;
  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S %z", "Jan 16 2019 18:23:12 CET");
  cr_expect(wct.wct_gmtoff == 1*3600, "Unexpected timezone offset: %ld, expected 1*3600", wct.wct_gmtoff);

  /* military zones */
  wct.wct_gmtoff = -1;
  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S %z", "Jan 16 2019 18:23:12 Z");
  cr_expect(wct.wct_gmtoff == 0, "Unexpected timezone offset: %ld, expected 0", wct.wct_gmtoff);
  wct.wct_gmtoff = -1;
  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S %z", "Jan 16 2019 18:23:12 M");
  cr_expect(wct.wct_gmtoff == -12*3600, "Unexpected timezone offset: %ld, expected -12*3600", wct.wct_gmtoff);
  wct.wct_gmtoff = -1;
  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S %z", "Jan 16 2019 18:23:12 Y");
  cr_expect(wct.wct_gmtoff == 12*3600, "Unexpected timezone offset: %ld, expected 12*3600", wct.wct_gmtoff);

  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S %z", "Jan 16 2019 18:23:12 J");
  cr_expect(end == NULL);

  /* hours only */
  wct.wct_gmtoff = -1;
  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S %z", "Jan 16 2019 18:23:12 +05");
  cr_expect(wct.wct_gmtoff == 5*3600, "Unexpected timezone offset: %ld, expected 5*3600", wct.wct_gmtoff);

  /* hours & minutes */
  wct.wct_gmtoff = -1;
  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S %z", "Jan 16 2019 18:23:12 +0500");
  cr_expect(wct.wct_gmtoff == 5*3600, "Unexpected timezone offset: %ld, expected 5*3600", wct.wct_gmtoff);
  wct.wct_gmtoff = -1;
  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S %z", "Jan 16 2019 18:23:12 +05:00");
  cr_expect(wct.wct_gmtoff == 5*3600, "Unexpected timezone offset: %ld, expected 5*3600", wct.wct_gmtoff);

  /* non-zero minutes */
  wct.wct_gmtoff = -1;
  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S %z", "Jan 16 2019 18:23:12 +05:30");
  cr_expect(wct.wct_gmtoff == 5*3600+30*60, "Unexpected timezone offset: %ld, expected 5*3600+30*60", wct.wct_gmtoff);
}

Test(wallclocktime, test_strptime_percent_Z_allows_timezone_to_be_optional)
{
  WallClockTime wct = WALL_CLOCK_TIME_INIT;
  gchar *end;

  /* NOTE: %Z accepts the same formats as %z, except that it allows the timezone to be optional */

  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S%Z", "Jan 16 2019 18:23:12PST");
  cr_assert(*end == 0);
  cr_expect(wct.wct_year == 119);
  cr_expect(wct.wct_mon == 0);
  cr_expect(wct.wct_mday == 16);

  cr_expect(wct.wct_hour == 18);
  cr_expect(wct.wct_min == 23);
  cr_expect(wct.wct_sec == 12);
  cr_expect(wct.wct_usec == 0);

  cr_expect(wct.wct_gmtoff == -8*3600, "Unexpected timezone offset: %ld, expected -8*3600", wct.wct_gmtoff);

  /* initial whitespace is not skipped */
  wct.wct_gmtoff = -1;
  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S%Z", "Jan 16 2019 18:23:12 PST");
  cr_expect(end != NULL);
  cr_expect_str_eq(end, " PST");

  wct.wct_gmtoff = -1;
  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S %Z", "Jan 16 2019 18:23:12 PST");
  cr_expect(wct.wct_gmtoff == -8*3600, "Unexpected timezone offset: %ld, expected -8*3600", wct.wct_gmtoff);

  wct.wct_gmtoff = -1;
  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S %Z", "Jan 16 2019 18:23:12");
  cr_expect(wct.wct_gmtoff == -1, "Unexpected timezone offset: %ld, expected -1", wct.wct_gmtoff);

  wct.wct_gmtoff = -1;
  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S %Z", "Jan 16 2019 18:23:12 Y");
  cr_expect(wct.wct_gmtoff == 12*3600, "Unexpected timezone offset: %ld, expected 12*3600", wct.wct_gmtoff);

  /* invalid timezone offset, too short */
  wct.wct_gmtoff = -1;
  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S %Z", "Jan 16 2019 18:23:12 +300");
  cr_expect(end != NULL);
  cr_expect_str_eq(end, "+300");

  wct.wct_gmtoff = -1;
  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S %Z", "Jan 16 2019 18:23:12 +3");
  cr_expect(end != NULL);
  cr_expect_str_eq(end, "+3");
}

Test(wallclocktime, test_strptime_zone_parsing_takes_daylight_saving_into_account_when_using_the_local_timezone)
{
  WallClockTime wct = WALL_CLOCK_TIME_INIT;
  gchar *end;

  /* this is a daylight saving zone name, in which case both wct.wct_isdst
   * and wct.wct_gmtoff must indicate this */
  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S %z", "May  7 2021 09:29:12 CEST");

  cr_assert(*end == 0);
  cr_expect(wct.wct_year == 121);
  cr_expect(wct.wct_mon == 4);
  cr_expect(wct.wct_mday == 7);

  cr_expect(wct.wct_hour == 9);
  cr_expect(wct.wct_min == 29);
  cr_expect(wct.wct_sec == 12);
  cr_expect(wct.wct_usec == 0);

  cr_expect(wct.wct_isdst > 0);
  cr_expect(wct.wct_gmtoff == 2*3600, "Unexpected timezone offset: %ld, expected 2*3600", wct.wct_gmtoff);

  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S %z", "Feb  7 2021 09:29:12 CET");

  cr_assert(*end == 0);
  cr_expect(wct.wct_year == 121);
  cr_expect(wct.wct_mon == 1);
  cr_expect(wct.wct_mday == 7);

  cr_expect(wct.wct_hour == 9);
  cr_expect(wct.wct_min == 29);
  cr_expect(wct.wct_sec == 12);
  cr_expect(wct.wct_usec == 0);

  cr_expect(wct.wct_isdst == 0);
  cr_expect(wct.wct_gmtoff == 1*3600, "Unexpected timezone offset: %ld, expected 1*3600", wct.wct_gmtoff);

}

static void
_guess_missing_year(WallClockTime *wct, gint mon)
{
  wct->wct_year = -1;
  wct->wct_mon = mon;
  wall_clock_time_guess_missing_year(wct);
}

static gboolean
_wct_year_matches(WallClockTime *wct, gint mon, gint ofs)
{
  gint faked_year = 2019;

  _guess_missing_year(wct, mon);
  return wct->wct_year == (faked_year - 1900) + ofs;
}

static gboolean
_guessed_year_is_next_year(WallClockTime *wct, gint mon)
{
  return _wct_year_matches(wct, mon, +1);
}

static gboolean
_guessed_year_is_current_year(WallClockTime *wct, gint mon)
{
  return _wct_year_matches(wct, mon, 0);
}

static gboolean
_guessed_year_is_last_year(WallClockTime *wct, gint mon)
{
  return _wct_year_matches(wct, mon, -1);
}

Test(wallclocktime, guess_year_returns_last_year)
{
  WallClockTime wct = WALL_CLOCK_TIME_INIT;

  gchar *end = wall_clock_time_strptime(&wct, "%b %d %H:%M:%S", "Jan 16 18:23:12");
  cr_assert(*end == 0);
  cr_assert(wct.wct_year == -1);

  /* Sat Jan 19 18:58:48 CET 2019 */
  fake_time(1547920728);

  for (gint mon = 0; mon < 11; mon++)
    cr_assert(_guessed_year_is_current_year(&wct, mon));

  /* december is assumed to be from the last year */
  cr_assert(_guessed_year_is_last_year(&wct, 11));
}

Test(wallclocktime, guess_year_returns_next_year)
{
  WallClockTime wct = WALL_CLOCK_TIME_INIT;

  gchar *end = wall_clock_time_strptime(&wct, "%b %d %H:%M:%S", "Jan 16 18:23:12");
  cr_assert(*end == 0);
  cr_assert(wct.wct_year == -1);

  /* Thu Dec 19 22:25:44 CET 2019 */
  fake_time(1576790744);

  /* january assumed to be from the future year */
  cr_assert(_guessed_year_is_next_year(&wct, 0));

  for (gint mon = 1; mon <= 11; mon++)
    cr_assert(_guessed_year_is_current_year(&wct, mon));
}

Test(wallclocktime, test_strptime_parses_without_date)
{
  WallClockTime wct = WALL_CLOCK_TIME_INIT;
  gchar *end;

  /* Thu Dec 19 22:25:44 CET 2019 */
  fake_time(1576790744);

  end = wall_clock_time_strptime(&wct, "%H:%M:%S %Z", "10:30:00 CET");
  wall_clock_time_guess_missing_fields(&wct);
  cr_expect(end != NULL);
  cr_expect(wct.wct_year == 119);
  cr_expect(wct.wct_mon == 11);
  cr_expect(wct.wct_mday == 19);
}

Test(wallclocktime, test_strptime_parses_without_mday)
{
  WallClockTime wct = WALL_CLOCK_TIME_INIT;
  gchar *end;

  /* Thu Dec 19 22:25:44 CET 2019 */
  fake_time(1576790744);

  end = wall_clock_time_strptime(&wct, "%Y-%m %H:%M:%S %Z", "2015-03 10:30:00 CET");
  wall_clock_time_guess_missing_fields(&wct);
  cr_expect(end != NULL);
  cr_expect(wct.wct_mday == 1);
}

Test(wallclocktime, test_strptime_parses_without_time)
{
  WallClockTime wct = WALL_CLOCK_TIME_INIT;
  gchar *end;

  /* Thu Dec 19 22:25:44 CET 2019 */
  fake_time(1576790744);

  end = wall_clock_time_strptime(&wct, "%Y-%m-%d %Z", "2015-03-01 CET");
  wall_clock_time_guess_missing_fields(&wct);
  cr_expect(end != NULL);
  cr_expect(wct.wct_hour == 0);
  cr_expect(wct.wct_min == 0);
  cr_expect(wct.wct_sec == 0);
}

Test(wallclocktime, test_strptime_parses_without_second)
{
  WallClockTime wct = WALL_CLOCK_TIME_INIT;
  gchar *end;

  /* Thu Dec 19 22:25:44 CET 2019 */
  fake_time(1576790744);

  end = wall_clock_time_strptime(&wct, "%Y-%m-%d %H:%M %Z", "2015-03-01 10:30 CET");
  wall_clock_time_guess_missing_fields(&wct);
  cr_expect(end != NULL);
  cr_expect(wct.wct_min == 30);
  cr_expect(wct.wct_sec == 0);
}

static void
setup(void)
{
  setenv("TZ", "CET", TRUE);
  invalidate_timeutils_cache();
}

static void
teardown(void)
{
}

TestSuite(wallclocktime, .init = setup, .fini = teardown);
