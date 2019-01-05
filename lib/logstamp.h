/*
 * Copyright (c) 2002-2011 Balabit
 * Copyright (c) 1998-2011 Balázs Scheidler
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

#ifndef LOGSTAMP_H_INCLUDED
#define LOGSTAMP_H_INCLUDED

#include "timeutils/unixtime.h"

/* timestamp formats */
#define TS_FMT_BSD   0
#define TS_FMT_ISO   1
#define TS_FMT_FULL  2
#define TS_FMT_UNIX  3

#define LOGSTAMP_ZONE_OFFSET_UNSET G_MININT32

typedef UnixTime LogStamp;

static inline gboolean
log_stamp_is_timezone_set(const LogStamp *self)
{
  return self->ut_gmtoff != -1;
}

void log_stamp_format(LogStamp *stamp, GString *target, gint ts_format, glong zone_offset, gint frac_digits);
void log_stamp_append_format(const LogStamp *stamp, GString *target, gint ts_format, glong zone_offset,
                             gint frac_digits);
gboolean log_stamp_eq(const LogStamp *a, const LogStamp *b);

#endif
