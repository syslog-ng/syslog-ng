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
#include "timeutils/unixtime.h"
#include "timeutils/wallclocktime.h"
#include "timeutils/cache.h"
#include "timeutils/names.h"
#include "timeutils/misc.h"

void
unix_time_unset(UnixTime *self)
{
  UnixTime val = UNIX_TIME_INIT;
  *self = val;
}

void
unix_time_set_now(UnixTime *self)
{
  GTimeVal tv;

  cached_g_current_time(&tv);
  self->ut_sec = tv.tv_sec;
  self->ut_usec = tv.tv_usec;
  self->ut_gmtoff = get_local_timezone_ofs(self->ut_sec);
}


gboolean
unix_time_eq(const UnixTime *a, const UnixTime *b)
{
  return a->ut_sec == b->ut_sec &&
         a->ut_usec == b->ut_usec &&
         a->ut_gmtoff == b->ut_gmtoff;
}
