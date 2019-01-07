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

void
wall_clock_time_unset(WallClockTime *self)
{
  WallClockTime val = WALL_CLOCK_TIME_INIT;
  *self = val;
}

gchar *
wall_clock_time_strptime(WallClockTime *wct, const gchar *format, const gchar *input)
{
  return strptime_with_tz(input, format, &wct->tm, &wct->wct_gmtoff, &wct->wct_zone);
}

void
wall_clock_time_set_from_unix_time(WallClockTime *self, const UnixTime *ut)
{
  wall_clock_time_set_from_unix_time_with_tz_override(self, ut, -1);
}

/* the timezone information overrides what is present in the timestamp, e.g.
 * it will _convert_ the timestamp to a destination timezone */
void
wall_clock_time_set_from_unix_time_with_tz_override(WallClockTime *self, const UnixTime *ut, gint gmtoff_override)
{
  gint gmtoff = gmtoff_override;

  if (gmtoff == -1)
    gmtoff = ut->ut_gmtoff;
  if (gmtoff == -1)
    gmtoff = get_local_timezone_ofs(ut->ut_sec);

  time_t t = ut->ut_sec + gmtoff;
  cached_gmtime_wct(&t, self);
  self->wct_gmtoff = gmtoff;
  self->wct_zone = NULL;
  self->wct_usec = ut->ut_usec;
}
