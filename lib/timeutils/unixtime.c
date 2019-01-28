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
#include "timeutils/unixtime.h"
#include "timeutils/timeutils.h"
#include "timeutils/wallclocktime.h"
#include "timeutils/cache.h"

void
unix_time_unset(UnixTime *self)
{
  UnixTime val = UNIX_TIME_INIT;
  *self = val;
}

void
unix_time_set_from_wall_clock_time(UnixTime *self, WallClockTime *wct)
{
  unix_time_set_from_wall_clock_time_with_tz_hint(self, wct, -1);
}

/* hint the timezone value if it is not present in the wct struct, e.g.  the
 * timestamp takes precedence, but as an additional information the caller
 * can supply its best idea.  this maps nicely to the source-side
 * time-zone() value, which is only used in case an incoming timestamp does
 * not have the timestamp value. */
void
unix_time_set_from_wall_clock_time_with_tz_hint(UnixTime *self, WallClockTime *wct, gint gmtoff_hint)
{
  self->ut_gmtoff = wct->wct_gmtoff;

  if (self->ut_gmtoff == -1)
    self->ut_gmtoff = gmtoff_hint;
  self->ut_usec = wct->wct_usec;

  /* FIRST: We convert the timestamp as it was in our local time zone. */
  gint unnormalized_hour = wct->wct_hour;
  wct->wct_isdst = -1;
  self->ut_sec = cached_mktime_wct(wct);
  gint normalized_hour = wct->wct_hour;

  if (self->ut_gmtoff == -1)
    self->ut_gmtoff = get_local_timezone_ofs(self->ut_sec);

  /* SECOND: adjust ut_sec as if we converted it according to our timezone. */
  self->ut_sec = self->ut_sec
                         + get_local_timezone_ofs(self->ut_sec)
                         - (normalized_hour - unnormalized_hour) * 3600
                         - self->ut_gmtoff;
}
