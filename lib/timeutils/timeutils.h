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

typedef struct _UnixTime UnixTime;
typedef struct _WallClockTime WallClockTime;

long get_local_timezone_ofs(time_t when);

gboolean check_nanosleep(void);

int format_zone_info(gchar *buf, size_t buflen, long gmtoff);
glong g_time_val_diff(GTimeVal *t1, GTimeVal *t2);
void timespec_add_msec(struct timespec *ts, glong msec);
glong timespec_diff_msec(const struct timespec *t1, const struct timespec *t2);
glong timespec_diff_nsec(struct timespec *t1, struct timespec *t2);

#endif
