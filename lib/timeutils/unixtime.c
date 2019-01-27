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
unix_time_set_from_wall_clock_time(UnixTime *self, const WallClockTime *wct)
{
  WallClockTime work_wct = *wct;
  unix_time_set_from_normalized_wall_clock_time(self, &work_wct);
}

void
unix_time_set_from_wall_clock_time_with_tz_hint(UnixTime *self, const WallClockTime *wct, long gmtoff_hint)
{
  WallClockTime work_wct = *wct;
  unix_time_set_from_normalized_wall_clock_time_with_tz_hint(self, &work_wct, gmtoff_hint);
}

void
unix_time_set_from_normalized_wall_clock_time(UnixTime *self, WallClockTime *wct)
{
  unix_time_set_from_normalized_wall_clock_time_with_tz_hint(self, wct, -1);
}

/* hint the timezone value if it is not present in the wct struct, e.g.  the
 * timestamp takes precedence, but as an additional information the caller
 * can supply its best idea.  this maps nicely to the source-side
 * time-zone() value, which is only used in case an incoming timestamp does
 * not have the timestamp value. */
void
unix_time_set_from_normalized_wall_clock_time_with_tz_hint(UnixTime *self, WallClockTime *wct, long gmtoff_hint)
{
  gint target_gmtoff = -1;

  /* usec is just copied over, doesn't change timezone or anything */
  self->ut_usec = wct->wct_usec;

  /* determine target gmtoff if it's coming from the timestamp or from the hint */
  target_gmtoff = wct->wct_gmtoff;
  if (target_gmtoff == -1)
    target_gmtoff = gmtoff_hint;

  /* FIRST: We convert the timestamp as it was in our local time zone. */
  gint unnormalized_hour = wct->wct_hour;
  wct->wct_isdst = -1;
  self->ut_sec = cached_mktime_wct(wct);
  gint normalized_hour = wct->wct_hour;


  /* SECOND: adjust ut_sec as if we converted it according to our timezone. */
  gint local_gmtoff = get_local_timezone_ofs(self->ut_sec);
  if (target_gmtoff == -1)
    {
      target_gmtoff = local_gmtoff;
    }
  self->ut_sec = self->ut_sec
                 + local_gmtoff
                 - (normalized_hour - unnormalized_hour) * 3600
                 - target_gmtoff;

  self->ut_gmtoff = target_gmtoff;
  wct->wct_gmtoff = self->ut_gmtoff;
  wct->wct_hour = unnormalized_hour;
}

void
unix_time_set_now(UnixTime *self)
{
  GTimeVal tv;

  cached_g_current_time(&tv);
  self->ut_sec = tv.tv_sec;
  self->ut_usec = tv.tv_usec;
  self->ut_gmtoff = get_local_timezone_ofs(self->ut_sec);
}
