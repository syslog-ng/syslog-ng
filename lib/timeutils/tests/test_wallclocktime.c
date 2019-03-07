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

  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S", "Jan 16 2019 18:23:12");
  cr_assert(*end == 0);
  cr_expect(wct.wct_year == 119);
  cr_expect(wct.wct_mon == 0);
  cr_expect(wct.wct_mday == 16);

  cr_expect(wct.wct_hour == 18);
  cr_expect(wct.wct_min == 23);
  cr_expect(wct.wct_sec == 12);

  cr_expect(wct.wct_gmtoff == -1);
}

Test(wallclocktime, test_strptime_parses_rfc822_timezone)
{
  WallClockTime wct = WALL_CLOCK_TIME_INIT;
  gchar *end;

  end = wall_clock_time_strptime(&wct, "%b %d %Y %H:%M:%S %z", "Jan 16 2019 18:23:12 PST");
  cr_assert(*end == 0);
  cr_expect(wct.wct_year == 119);
  cr_expect(wct.wct_mon == 0);
  cr_expect(wct.wct_mday == 16);

  cr_expect(wct.wct_hour == 18);
  cr_expect(wct.wct_min == 23);
  cr_expect(wct.wct_sec == 12);

  cr_expect(wct.wct_gmtoff == -8*3600, "Unexpected timezone offset: %ld", wct.wct_gmtoff);
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

TestSuite(wallclocktime, .init = setup, .fini = teardown);
