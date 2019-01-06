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

#ifndef WALLCLOCKTIME_H_INCLUDED
#define WALLCLOCKTIME_H_INCLUDED

#include "timeutils/timeutils.h"

/*
 * This is a simple wrapper over "struct tm" with fields that are not
 * portable across all platforms or are not present at all, but which we
 * would use.
 *
 * For instance, wct_gmtoff is not present on a number of platforms but would
 * be very useful internally, so we _always_ have the capability to
 * represent a broken-down time with all the properties needed for proper
 * timezone reference.
 *
 * Another example is wct_usec, which represents the number of microseconds
 * so we can carry fractions of a second information.
 *
 * The design of the class is so that we don't need to copy the contents of
 * "struct tm" around for conversions, rather we contain one instance and
 * provide wrapper macros so that fields that are present in struct tm are
 * used from there, those that aren't will be part of the wrapper structure.
 */
struct _WallClockTime
{
#define wct_year  tm.tm_year
#define wct_mon   tm.tm_mon
#define wct_mday  tm.tm_mday
#define wct_wday  tm.tm_wday
#define wct_yday  tm.tm_yday
#define wct_hour  tm.tm_hour
#define wct_min   tm.tm_min
#define wct_sec   tm.tm_sec
#define wct_isdst tm.tm_isdst
  struct tm tm;


  /* We might need to separate tm_zone to a different conditional/autoconf
   * check.  At least on Linux/FreeBSD they were introduced the same time.
   * Some platforms may lack tm_zone, even though they have tm_gmtoff */

#ifndef SYSLOG_NG_HAVE_STRUCT_TM_TM_GMTOFF
  long wct_gmtoff;
  const char *wct_zone;
#else
#define wct_gmtoff tm.tm_gmtoff
#define wct_zone tm.tm_zone
#endif
  int wct_usec;
};

#ifdef SYSLOG_NG_HAVE_STRUCT_TM_TM_GMTOFF

#define WALL_CLOCK_TIME_INIT \
  {                                     \
    .tm =                               \
      {                                 \
        .tm_year = -1,                  \
        .tm_mon = -1,                   \
        .tm_mday = -1,                  \
        .tm_wday = -1,                  \
        .tm_yday = -1,                  \
        .tm_hour = -1,                  \
        .tm_min = -1,                   \
        .tm_sec = -1,                   \
        .tm_isdst = -1,                 \
        .tm_gmtoff = -1,                \
        .tm_zone = NULL,                \
      },                                \
      .wct_usec = 0,                    \
  }
#else

#define WALL_CLOCK_TIME_INIT \
  {                                     \
    .tm =                               \
      {                                 \
        .tm_year = -1,                  \
        .tm_mon = -1,                   \
        .tm_mday = -1,                  \
        .tm_wday = -1,                  \
        .tm_yday = -1,                  \
        .tm_hour = -1,                  \
        .tm_min = -1,                   \
        .tm_sec = -1,                   \
        .tm_isdst = -1,                 \
      },                                \
      .wct_gmtoff = -1,                 \
      .wct_zone = NULL,                 \
      .wct_usec = 0,                    \
  }

#endif

static inline gboolean
wall_clock_time_is_set(WallClockTime *wct)
{
  return wct->wct_hour != -1;
}

void wall_clock_time_unset(WallClockTime *wct);
gchar *wall_clock_time_strptime(WallClockTime *wct, const gchar *format, const gchar *input);

#endif
