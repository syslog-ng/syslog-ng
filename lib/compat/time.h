/*
 * Copyright (c) 2014 Balabit
 * Copyright (c) 2014 Gergely Nagy <algernon@madhouse-project.org>
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

#ifndef COMPAT_TIME_H_INCLUDED
#define COMPAT_TIME_H_INCLUDED

#include "compat/compat.h"
#include <time.h>

#if !defined(SYSLOG_NG_HAVE_CLOCK_GETTIME) && defined(__APPLE__) && defined(__MACH__)

#include <mach/clock.h>
#include <mach/mach.h>

#define CLOCK_REALTIME CALENDAR_CLOCK
#define CLOCK_MONOTONIC SYSTEM_CLOCK

int clock_gettime(clock_t clock_id, struct timespec *timestamp);

#else

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC CLOCK_REALTIME
#endif

#endif /* !SYSLOG_NG_HAVE_CLOCK_GETTIME && __APPLE__ && __MACH__ */

#endif /* COMPAT_TIME_H_INCLUDED */
