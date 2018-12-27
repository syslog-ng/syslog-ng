/*
 * Copyright (c) 2002-2011 Balabit
 * Copyright (c) 1998-2011 Balázs Scheidler
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

#include "logstamp.h"
#include "messages.h"
#include "timeutils/timeutils.h"
#include "str-format.h"

static void
log_stamp_append_frac_digits(const LogStamp *stamp, GString *target, gint frac_digits)
{
  glong usecs;

  usecs = stamp->tv_usec % 1000000;

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
 * log_stamp_format:
 * @stamp: Timestamp to format
 * @target: Target storage for formatted timestamp
 * @ts_format: Specifies basic timestamp format (TS_FMT_BSD, TS_FMT_ISO)
 * @zone_offset: Specifies custom zone offset if @tz_convert == TZ_CNV_CUSTOM
 *
 * Emits the formatted version of @stamp into @target as specified by
 * @ts_format and @tz_convert.
 **/
void
log_stamp_append_format(const LogStamp *stamp, GString *target, gint ts_format, glong zone_offset, gint frac_digits)
{
  glong target_zone_offset = 0;
  struct tm *tm, tm_storage;
  char buf[8];
  time_t t;

  if (zone_offset != -1)
    target_zone_offset = zone_offset;
  else
    target_zone_offset = stamp->zone_offset;

  t = stamp->tv_sec + target_zone_offset;
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
      log_stamp_append_frac_digits(stamp, target, frac_digits);
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

      log_stamp_append_frac_digits(stamp, target, frac_digits);
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

      log_stamp_append_frac_digits(stamp, target, frac_digits);
      break;
    case TS_FMT_UNIX:
      format_uint32_padded(target, 0, 0, 10, (int) stamp->tv_sec);
      log_stamp_append_frac_digits(stamp, target, frac_digits);
      break;
    default:
      g_assert_not_reached();
      break;
    }
}

void
log_stamp_format(LogStamp *stamp, GString *target, gint ts_format, glong zone_offset, gint frac_digits)
{
  g_string_truncate(target, 0);
  log_stamp_append_format(stamp, target, ts_format, zone_offset, frac_digits);
}

gboolean
log_stamp_eq(const LogStamp *a, const LogStamp *b)
{
  return a->tv_sec == b->tv_sec &&
         a->tv_usec == b->tv_usec &&
         a->zone_offset == b->zone_offset;
}
