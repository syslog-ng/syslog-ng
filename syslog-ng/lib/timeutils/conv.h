/*
 * Copyright (c) 2002-2019 Balabit
 * Copyright (c) 1998-2019 Balázs Scheidler
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

#ifndef TIMEUTILS_CONV_H_INCLUDED
#define TIMEUTILS_CONV_H_INCLUDED

#include "timeutils/unixtime.h"
#include "timeutils/wallclocktime.h"

/*
 * The algorithm that takes a WallClockTime and converts it to UnixTime
 * mutates the WallClockTime, contrary to intuitions (this behavior is
 * inherited from mktime, which we use in the implementation).  We therefore
 * introduce a second set of set_from() functions, so we have two sets:
 *
 * 1) one that takes a const WallClockTime
 * 2) one that mutates its argument
 *
 * The second one is qualified with the word "normalized".
 */
void convert_wall_clock_time_to_unix_time(const WallClockTime *src, UnixTime *dst);
void convert_wall_clock_time_to_unix_time_with_tz_hint(const WallClockTime *src, UnixTime *dst, long gmtoff_hint);

/* these change the WallClockTime while setting unix time, just as mktime() would */
void convert_and_normalize_wall_clock_time_to_unix_time(WallClockTime *src, UnixTime *dst);
void convert_and_normalize_wall_clock_time_to_unix_time_with_tz_hint(WallClockTime *src, UnixTime *dst,
    long gmtoff_hint);

void convert_unix_time_to_wall_clock_time(const UnixTime *src, WallClockTime *dst);
void convert_unix_time_to_wall_clock_time_with_tz_override(const UnixTime *src, WallClockTime *dst,
                                                           gint gmtoff_override);

#endif
