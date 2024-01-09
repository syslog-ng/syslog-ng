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

#ifndef TIMEUTILS_MISC_H_INCLUDED
#define TIMEUTILS_MISC_H_INCLUDED

#include "syslog-ng.h"

#if ! defined(NSEC_PER_SEC)
# define NSEC_PER_SEC 1000000000L
# define USEC_PER_SEC 1000000L
# define MSEC_PER_SEC 1000L
#endif

#define MSEC_TO_NSEC(msec)   ((msec) * NSEC_PER_SEC / MSEC_PER_SEC)
#define MSEC_TO_USEC(msec)   ((msec) * USEC_PER_SEC / MSEC_PER_SEC)

#define USEC_TO_NSEC(usec)   ((usec) * NSEC_PER_SEC / USEC_PER_SEC)
#define USEC_TO_USEC(usec)   ((usec) * USEC_PER_SEC / USEC_PER_SEC)

gboolean check_nanosleep(void);

void timespec_add_msec(struct timespec *ts, glong msec);
void timespec_add_usec(struct timespec *ts, glong usec);
glong timespec_diff_msec(const struct timespec *t1, const struct timespec *t2);
glong timespec_diff_usec(const struct timespec *t1, const struct timespec *t2);
glong timespec_diff_nsec(struct timespec *t1, struct timespec *t2);

#endif
