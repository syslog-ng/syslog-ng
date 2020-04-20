/*
 * Copyright (c) 2016 Balabit
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

#include "compat/time.h"

#if !defined(SYSLOG_NG_HAVE_CLOCK_GETTIME) && defined(__APPLE__) && defined(__MACH__)

int
clock_gettime(clock_t clock_id, struct timespec *timestamp)
{
  clock_serv_t clock_server;
  mach_timespec_t mach_timestamp;

  host_get_clock_service(mach_host_self(), clock_id, &clock_server);
  clock_get_time(clock_server, &mach_timestamp);

  timestamp->tv_sec = mach_timestamp.tv_sec;
  timestamp->tv_nsec = mach_timestamp.tv_nsec;

  mach_port_deallocate(mach_task_self(), clock_server);
  return 0;
}

#endif
