/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "logstamp.h"
#include "messages.h"
#include "timeutils.h"

static void
log_stamp_append_frac_digits(LogStamp *stamp, GString *target, gint frac_digits)
{
  glong usecs;

  usecs = stamp->time.tv_usec % 1000000;

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
log_stamp_append_format(LogStamp *stamp, GString *target, gint ts_format, glong zone_offset, gint frac_digits)
{
  glong target_zone_offset = 0;
  struct tm *tm, tm_storage;
  char buf[8];
  time_t t;
  
  if (zone_offset != -1)
    target_zone_offset = zone_offset;
  else
    target_zone_offset = stamp->zone_offset;

  t = stamp->time.tv_sec + target_zone_offset;
  if (!(tm = gmtime_r(&t, &tm_storage))) 
    {
      /* this should never happen */
      g_string_sprintf(target, "%d", (int) stamp->time.tv_sec);
      msg_error("Error formatting time stamp, gmtime() failed",
                evt_tag_int("stamp", (int) t),
                NULL);
    } 
  else 
    {
      switch (ts_format)
        {
        case TS_FMT_BSD:
          g_string_append_printf(target, "%s %2d %02d:%02d:%02d", month_names_abbrev[tm->tm_mon], tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
          log_stamp_append_frac_digits(stamp, target, frac_digits);
          break;
        case TS_FMT_ISO:
          g_string_append_printf(target, "%d-%02d-%02dT%02d:%02d:%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
          log_stamp_append_frac_digits(stamp, target, frac_digits);
          format_zone_info(buf, sizeof(buf), target_zone_offset);
          g_string_append(target, buf);
          break;
        case TS_FMT_FULL:
          g_string_append_printf(target, "%d %s %2d %02d:%02d:%02d", tm->tm_year + 1900, month_names_abbrev[tm->tm_mon], tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
          log_stamp_append_frac_digits(stamp, target, frac_digits);
          break;
        case TS_FMT_UNIX:
          g_string_append_printf(target, "%d", (int) stamp->time.tv_sec);
          log_stamp_append_frac_digits(stamp, target, frac_digits);
          break;
        default:
          g_assert_not_reached();
          break;
        }
    }
}

void
log_stamp_format(LogStamp *stamp, GString *target, gint ts_format, glong zone_offset, gint frac_digits)
{
  g_string_truncate(target, 0);
  log_stamp_append_format(stamp, target, ts_format, zone_offset, frac_digits);
}
