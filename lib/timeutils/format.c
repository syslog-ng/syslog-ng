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
#include "timeutils/format.h"
#include "timeutils/cache.h"
#include "timeutils/names.h"
#include "timeutils/conv.h"
#include "str-format.h"

static void
_append_frac_digits(glong usecs, GString *target, gint frac_digits)
{
  usecs = usecs % 1000000;

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

static gint
_format_zone_info(gchar *buf, size_t buflen, glong gmtoff)
{
  return g_snprintf(buf, buflen, "%c%02ld:%02ld",
                    gmtoff < 0 ? '-' : '+',
                    (gmtoff < 0 ? -gmtoff : gmtoff) / 3600,
                    ((gmtoff < 0 ? -gmtoff : gmtoff) % 3600) / 60);
}

void
append_format_unix_time(const UnixTime *ut, GString *target, gint ts_format, glong zone_offset, gint frac_digits)
{
  WallClockTime wct = WALL_CLOCK_TIME_INIT;

  if (ts_format == TS_FMT_UNIX)
    {
      format_uint32_padded(target, 0, 0, 10, (int) ut->ut_sec);
      _append_frac_digits(ut->ut_usec, target, frac_digits);
    }
  else
    {
      wall_clock_time_set_from_unix_time_with_tz_override(&wct, ut, zone_offset);
      append_format_wall_clock_time(&wct, target, ts_format, frac_digits);
    }
}

void
format_unix_time(const UnixTime *stamp, GString *target, gint ts_format, glong zone_offset, gint frac_digits)
{
  g_string_truncate(target, 0);
  append_format_unix_time(stamp, target, ts_format, zone_offset, frac_digits);
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
append_format_wall_clock_time(const WallClockTime *wct, GString *target, gint ts_format, gint frac_digits)
{
  UnixTime ut = UNIX_TIME_INIT;
  char buf[8];

  switch (ts_format)
    {
    case TS_FMT_BSD:
      g_string_append(target, month_names_abbrev[wct->wct_mon]);
      g_string_append_c(target, ' ');
      format_uint32_padded(target, 2, ' ', 10, wct->wct_mday);
      g_string_append_c(target, ' ');
      format_uint32_padded(target, 2, '0', 10, wct->wct_hour);
      g_string_append_c(target, ':');
      format_uint32_padded(target, 2, '0', 10, wct->wct_min);
      g_string_append_c(target, ':');
      format_uint32_padded(target, 2, '0', 10, wct->wct_sec);
      _append_frac_digits(wct->wct_usec, target, frac_digits);
      break;
    case TS_FMT_ISO:
      format_uint32_padded(target, 0, 0, 10, wct->wct_year + 1900);
      g_string_append_c(target, '-');
      format_uint32_padded(target, 2, '0', 10, wct->wct_mon + 1);
      g_string_append_c(target, '-');
      format_uint32_padded(target, 2, '0', 10, wct->wct_mday);
      g_string_append_c(target, 'T');
      format_uint32_padded(target, 2, '0', 10, wct->wct_hour);
      g_string_append_c(target, ':');
      format_uint32_padded(target, 2, '0', 10, wct->wct_min);
      g_string_append_c(target, ':');
      format_uint32_padded(target, 2, '0', 10, wct->wct_sec);

      _append_frac_digits(wct->wct_usec, target, frac_digits);
      _format_zone_info(buf, sizeof(buf), wct->wct_gmtoff);
      g_string_append(target, buf);
      break;
    case TS_FMT_FULL:
      format_uint32_padded(target, 0, 0, 10, wct->wct_year + 1900);
      g_string_append_c(target, ' ');
      g_string_append(target, month_names_abbrev[wct->wct_mon]);
      g_string_append_c(target, ' ');
      format_uint32_padded(target, 2, ' ', 10, wct->wct_mday);
      g_string_append_c(target, ' ');
      format_uint32_padded(target, 2, '0', 10, wct->wct_hour);
      g_string_append_c(target, ':');
      format_uint32_padded(target, 2, '0', 10, wct->wct_min);
      g_string_append_c(target, ':');
      format_uint32_padded(target, 2, '0', 10, wct->wct_sec);

      _append_frac_digits(wct->wct_usec, target, frac_digits);
      break;
    case TS_FMT_UNIX:
      unix_time_set_from_wall_clock_time(&ut, wct);
      append_format_unix_time(&ut, target, TS_FMT_UNIX, wct->wct_gmtoff, frac_digits);
      break;
    default:
      g_assert_not_reached();
      break;
    }
}


gint
format_zone_info(gchar *buf, size_t buflen, glong gmtoff)
{
  return g_snprintf(buf, buflen, "%c%02ld:%02ld",
                    gmtoff < 0 ? '-' : '+',
                    (gmtoff < 0 ? -gmtoff : gmtoff) / 3600,
                    ((gmtoff < 0 ? -gmtoff : gmtoff) % 3600) / 60);
}
