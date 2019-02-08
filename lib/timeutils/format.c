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
#include "str-format.h"

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

static gint
_format_zone_info(gchar *buf, size_t buflen, glong gmtoff)
{
  return g_snprintf(buf, buflen, "%c%02ld:%02ld",
                    gmtoff < 0 ? '-' : '+',
                    (gmtoff < 0 ? -gmtoff : gmtoff) / 3600,
                    ((gmtoff < 0 ? -gmtoff : gmtoff) % 3600) / 60);
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
append_format_unix_time(const UnixTime *stamp, GString *target, gint ts_format, glong zone_offset, gint frac_digits)
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
      _format_zone_info(buf, sizeof(buf), target_zone_offset);
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
format_unix_time(const UnixTime *stamp, GString *target, gint ts_format, glong zone_offset, gint frac_digits)
{
  g_string_truncate(target, 0);
  append_format_unix_time(stamp, target, ts_format, zone_offset, frac_digits);
}

gint
format_zone_info(gchar *buf, size_t buflen, glong gmtoff)
{
  return g_snprintf(buf, buflen, "%c%02ld:%02ld",
                    gmtoff < 0 ? '-' : '+',
                    (gmtoff < 0 ? -gmtoff : gmtoff) / 3600,
                    ((gmtoff < 0 ? -gmtoff : gmtoff) % 3600) / 60);
}
