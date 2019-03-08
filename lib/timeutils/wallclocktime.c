/*
 * Copyright (c) 2002-2018 Balabit
 * Copyright (c) 1998-2018 Balázs Scheidler
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

#include "timeutils/wallclocktime.h"
#include "timeutils/unixtime.h"
#include "timeutils/strptime-tz.h"
#include "timeutils/cache.h"
#include "timeutils/misc.h"

void
wall_clock_time_unset(WallClockTime *self)
{
  WallClockTime val = WALL_CLOCK_TIME_INIT;
  *self = val;
}

gchar *
wall_clock_time_strptime(WallClockTime *wct, const gchar *format, const gchar *input)
{
  return strptime_with_tz(input, format, &wct->tm, &wct->wct_gmtoff, (const char **) &wct->wct_zone);
}

/* Determine (guess) the year for the month.
 *
 * It can be used for BSD logs, where year is missing.
 */
static gint
determine_year_for_month(gint month, const struct tm *now)
{
  if (month == 11 && now->tm_mon == 0)
    return now->tm_year - 1;
  else if (month == 0 && now->tm_mon == 11)
    return now->tm_year + 1;
  else
    return now->tm_year;
}

void
wall_clock_time_guess_missing_year(WallClockTime *self)
{
  if (self->wct_year == -1)
    {
      time_t now;
      struct tm tm;

      now = cached_g_current_time_sec();
      cached_localtime(&now, &tm);
      self->wct_year = determine_year_for_month(self->wct_mon, &tm);
    }
}
