/*
 * Copyright (c) 2002-2014 Balabit
 * Copyright (c) 1998-2010 Balázs Scheidler
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

#ifndef TIMEUTILS_H_INCLUDED
#define TIMEUTILS_H_INCLUDED

#include "syslog-ng.h"
#include "compat/time.h"

time_t cached_mktime(struct tm *tm);
void cached_localtime(time_t *when, struct tm *tm);
void cached_gmtime(time_t *when, struct tm *tm);

long get_local_timezone_ofs(time_t when);
void clean_time_cache(void);


void invalidate_cached_time(void);
void set_cached_time(GTimeVal *timeval);
void cached_g_current_time(GTimeVal *result);
time_t cached_g_current_time_sec(void);

gboolean check_nanosleep(void);

int format_zone_info(gchar *buf, size_t buflen, long gmtoff);
glong g_time_val_diff(GTimeVal *t1, GTimeVal *t2);
void timespec_add_msec(struct timespec *ts, glong msec);
glong timespec_diff_msec(const struct timespec *t1, const struct timespec *t2);
glong timespec_diff_nsec(struct timespec *t1, struct timespec *t2);
gint determine_year_for_month(gint month, const struct tm *now);

typedef struct _ZoneInfo ZoneInfo;
typedef struct _TimeZoneInfo TimeZoneInfo;

gint32 time_zone_info_get_offset(const TimeZoneInfo *self, time_t stamp);
TimeZoneInfo *time_zone_info_new(const gchar *tz);
void time_zone_info_free(TimeZoneInfo *self);

extern const char *month_names_abbrev[];
extern const char *month_names[];
extern const char *weekday_names_abbrev[];
extern const char *weekday_names[];


#endif
