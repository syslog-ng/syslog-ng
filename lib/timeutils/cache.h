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

#ifndef TIMEUTILS_CACHE_H_INCLUDED
#define TIMEUTILS_CACHE_H_INCLUDED

#include "timeutils/wallclocktime.h"
#include "timeutils/zoneinfo.h"

/* the thread safe variant of the global "timezone" */
glong cached_get_system_tzofs(void);

/* the thread safe variant of the global "tzname" */
const gchar *const *cached_get_system_tznames(void);

/* the timezone offset at the specified UTC time */
long get_local_timezone_ofs(time_t when);

time_t cached_mktime(struct tm *tm);
void cached_localtime(time_t *when, struct tm *tm);
void cached_gmtime(time_t *when, struct tm *tm);

void timeutils_cache_deinit(void);

static inline void
cached_localtime_wct(time_t *when, WallClockTime *wct)
{
  cached_localtime(when, &wct->tm);
}

static inline time_t
cached_mktime_wct(WallClockTime *wct)
{
  return cached_mktime(&wct->tm);
}

static inline void
cached_gmtime_wct(time_t *when, WallClockTime *wct)
{
  cached_gmtime(when, &wct->tm);
}

void invalidate_cached_realtime(void);
void set_cached_realtime(struct timespec *ts);
void get_cached_realtime(struct timespec *ts);
time_t get_cached_realtime_sec(void);

TimeZoneInfo *cached_get_time_zone_info(const gchar *tz);

void invalidate_timeutils_cache(void);

#endif
