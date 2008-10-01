/*
 * Copyright (c) 2002-2008 BalaBit IT Ltd, Budapest, Hungary
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
  
#ifndef TIMEUTILS_H_INCLUDED
#define TIMEUTILS_H_INCLUDED

#include "syslog-ng.h"
#include <time.h>

extern const char *month_names[];
extern const char *weekday_names[];

time_t cached_mktime(struct tm *tm);
void cached_localtime(time_t *when, struct tm *tm);
long get_local_timezone_ofs(time_t when);
void clean_time_cache();

int format_zone_info(gchar *buf, size_t buflen, long gmtoff);
long get_local_timezone_ofs(time_t when);
glong g_time_val_diff(GTimeVal *t1, GTimeVal *t2);

typedef struct _ZoneInfo ZoneInfo;
typedef struct _TimeZoneInfo TimeZoneInfo;

gint32 time_zone_info_get_offset(const TimeZoneInfo *self, time_t stamp);
TimeZoneInfo* time_zone_info_new(const gchar *tz);
void time_zone_info_free(TimeZoneInfo *self);

#endif
