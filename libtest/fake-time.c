/*
 * Copyright (c) 2019 Balabit
 * Copyright (c) 2019 Balázs Scheidler
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
#include "fake-time.h"
#include "timeutils/cache.h"
#include <iv.h>

void
fake_time(time_t now)
{
  struct timespec ts = { now, 123LL * 1000000LL };
  set_cached_realtime(&ts);
}

void
fake_time_add(time_t diff)
{
  fake_time(get_cached_realtime_sec() + diff);

  /* HACK to bump iv_now */
  struct timespec *writable_iv_now = (struct timespec *) &iv_now;
  iv_validate_now();
  writable_iv_now->tv_sec += diff;
}
