/*
 * Copyright (c) 2002, 2003, 2004 BalaBit IT Ltd, Budapest, Hungary
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

#ifndef MACROS_H_INCLUDED
#define MACROS_H_INCLUDED

#include "syslog-ng.h"
#include "logmsg.h"

/* macro expansion flags */
#define MF_ESCAPE_RESULT  0x0001
#define MF_STAMP_RECVD    0x0002

/* macro IDs */
#define M_NONE     0

#define M_FACILITY 1
#define M_LEVEL    2
#define M_TAG      3
#define M_PRI      4

#define M_DATE     10
#define M_FULLDATE 11
#define M_ISODATE  12
#define M_STAMP    13
#define M_YEAR     14
#define M_MONTH    15
#define M_DAY      16
#define M_HOUR     17
#define M_MIN      18
#define M_SEC      19
#define M_WEEKDAY  20
#define M_WEEK     21
#define M_TZOFFSET 22
#define M_TZ       23
#define M_UNIXTIME 24

#define M_DATE_RECVD     30
#define M_FULLDATE_RECVD 31
#define M_ISODATE_RECVD  32
#define M_STAMP_RECVD    33
#define M_YEAR_RECVD     34
#define M_MONTH_RECVD    35
#define M_DAY_RECVD      36
#define M_HOUR_RECVD     37
#define M_MIN_RECVD      38
#define M_SEC_RECVD      39
#define M_WEEKDAY_RECVD  40
#define M_WEEK_RECVD     41
#define M_TZOFFSET_RECVD 42
#define M_TZ_RECVD       43
#define M_UNIXTIME_RECVD 44

#define M_DATE_STAMP     50
#define M_FULLDATE_STAMP 51
#define M_ISODATE_STAMP  52
#define M_STAMP_STAMP    53
#define M_YEAR_STAMP     54
#define M_MONTH_STAMP    55
#define M_DAY_STAMP      56
#define M_HOUR_STAMP     57
#define M_MIN_STAMP      58
#define M_SEC_STAMP      59
#define M_WEEKDAY_STAMP  60
#define M_WEEK_STAMP     61
#define M_TZOFFSET_STAMP 62
#define M_TZ_STAMP       63
#define M_UNIXTIME_STAMP 64

#define M_FULLHOST       70
#define M_HOST           71
#define M_FULLHOST_FROM  72
#define M_HOST_FROM      73
#define M_PROGRAM        74

#define M_MESSAGE        75
#define M_MSGONLY	 76
#define M_SOURCE_IP      77

#define M_MATCH_REF_OFS 256

guint
log_macro_lookup(gchar *macro, gint len);

gboolean
log_macro_expand(GString *result, gint id, guint32 flags, gint ts_format, glong zone_offset, LogMessage *msg);

#endif
