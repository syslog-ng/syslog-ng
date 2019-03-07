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

#ifndef UNIXTIME_H_INCLUDED
#define UNIXTIME_H_INCLUDED

#include "syslog-ng.h"

/*
 * This class represents a UNIX timestamp (as measured in time_t), the
 * fractions of a second (in microseconds) and an associated GMT timezone
 * offset.  In concept it is the combination of "struct timeval" and the
 * number of seconds compared to GMT.
 *
 * In concept, this is similar to WallClockTime, with the difference that
 * this represents the time in seconds since the UNIX epoch.
 *
 * In design, it is also similar to WallClockTime, the original intention
 * was to use an embedded "struct timeval", however that would enlarge the
 * LogMessage structure with a lot of padding (the struct would go from 16
 * to 24 bytes and we have 3 of these structs in LogMessage).
 */
typedef struct _UnixTime UnixTime;
struct _UnixTime
{
  gint64 ut_sec;
  guint32 ut_usec;

  /* zone offset in seconds, add this to UTC to get the time in local.  This
   * is just 32 bits, contrary to all other gmtoff variables, as we are
   * squeezed in space with this struct.  32 bit is more than enough for
   * +/-24*3600 */
  gint32 ut_gmtoff;
};

#define UNIX_TIME_INIT { -1, 0, -1 }

static inline gboolean
unix_time_is_set(const UnixTime *ut)
{
  return ut->ut_sec != -1;
}

static inline gboolean
unix_time_is_timezone_set(const UnixTime *self)
{
  return self->ut_gmtoff != -1;
}


void unix_time_unset(UnixTime *ut);

void unix_time_set_now(UnixTime *self);

gboolean unix_time_eq(const UnixTime *a, const UnixTime *b);


#endif
