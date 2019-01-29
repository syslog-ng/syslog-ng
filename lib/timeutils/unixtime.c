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
#include "timeutils/names.h"
#include "str-format.h"

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

static void
_append_frac_digits(const UnixTime *stamp, GString *target, gint frac_digits)
{
  glong usecs;

  usecs = stamp->ut_usec % 1000000;

  if (frac_digits > 0)
    {
      gulong x;

      g_string_append_c(target, '.');
      for (x = 100000; frac_digits && x; x = x / 10)
        {
          g_string_append_c(target, (usecs / x) + '0');
          usecs = usecs % x;
          frac_digits--;
        }
    }
}

/**
 * unix_time_format:
 * @stamp: Timestamp to format
 * @target: Target storage for formatted timestamp
 * @ts_format: Specifies basic timestamp format (TS_FMT_BSD, TS_FMT_ISO)
 * @zone_offset: Specifies custom zone offset if @tz_convert == TZ_CNV_CUSTOM
 *
 * Emits the formatted version of @stamp into @target as specified by
 * @ts_format and @tz_convert.
 **/
void
unix_time_append_format(const UnixTime *stamp, GString *target, gint ts_format, glong zone_offset, gint frac_digits)
{
  glong target_zone_offset = 0;
  struct tm *tm, tm_storage;
  char buf[8];
  time_t t;

  if (zone_offset != -1)
    target_zone_offset = zone_offset;
  else
    target_zone_offset = stamp->ut_gmtoff;

  t = stamp->ut_sec + target_zone_offset;
  cached_gmtime(&t, &tm_storage);
  tm = &tm_storage;
  switch (ts_format)
    {
    case TS_FMT_BSD:
      g_string_append(target, month_names_abbrev[tm->tm_mon]);
      g_string_append_c(target, ' ');
      format_uint32_padded(target, 2, ' ', 10, tm->tm_mday);
      g_string_append_c(target, ' ');
      format_uint32_padded(target, 2, '0', 10, tm->tm_hour);
      g_string_append_c(target, ':');
      format_uint32_padded(target, 2, '0', 10, tm->tm_min);
      g_string_append_c(target, ':');
      format_uint32_padded(target, 2, '0', 10, tm->tm_sec);
      _append_frac_digits(stamp, target, frac_digits);
      break;
    case TS_FMT_ISO:
      format_uint32_padded(target, 0, 0, 10, tm->tm_year + 1900);
      g_string_append_c(target, '-');
      format_uint32_padded(target, 2, '0', 10, tm->tm_mon + 1);
      g_string_append_c(target, '-');
      format_uint32_padded(target, 2, '0', 10, tm->tm_mday);
      g_string_append_c(target, 'T');
      format_uint32_padded(target, 2, '0', 10, tm->tm_hour);
      g_string_append_c(target, ':');
      format_uint32_padded(target, 2, '0', 10, tm->tm_min);
      g_string_append_c(target, ':');
      format_uint32_padded(target, 2, '0', 10, tm->tm_sec);

      _append_frac_digits(stamp, target, frac_digits);
      format_zone_info(buf, sizeof(buf), target_zone_offset);
      g_string_append(target, buf);
      break;
    case TS_FMT_FULL:
      format_uint32_padded(target, 0, 0, 10, tm->tm_year + 1900);
      g_string_append_c(target, ' ');
      g_string_append(target, month_names_abbrev[tm->tm_mon]);
      g_string_append_c(target, ' ');
      format_uint32_padded(target, 2, ' ', 10, tm->tm_mday);
      g_string_append_c(target, ' ');
      format_uint32_padded(target, 2, '0', 10, tm->tm_hour);
      g_string_append_c(target, ':');
      format_uint32_padded(target, 2, '0', 10, tm->tm_min);
      g_string_append_c(target, ':');
      format_uint32_padded(target, 2, '0', 10, tm->tm_sec);

      _append_frac_digits(stamp, target, frac_digits);
      break;
    case TS_FMT_UNIX:
      format_uint32_padded(target, 0, 0, 10, (int) stamp->ut_sec);
      _append_frac_digits(stamp, target, frac_digits);
      break;
    default:
      g_assert_not_reached();
      break;
    }
}

void
unix_time_format(UnixTime *stamp, GString *target, gint ts_format, glong zone_offset, gint frac_digits)
{
  g_string_truncate(target, 0);
  unix_time_append_format(stamp, target, ts_format, zone_offset, frac_digits);
}

gboolean
unix_time_eq(const UnixTime *a, const UnixTime *b)
{
  return a->ut_sec == b->ut_sec &&
         a->ut_usec == b->ut_usec &&
         a->ut_gmtoff == b->ut_gmtoff;
}
