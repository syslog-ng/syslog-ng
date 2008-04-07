/*
 * Copyright (c) 2002-2007 BalaBit IT Ltd, Budapest, Hungary                    
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
enum
{
  M_NONE,

  M_FACILITY,
  M_FACILITY_NUM,
  M_LEVEL,
  M_LEVEL_NUM,
  M_TAG,
  M_BSDTAG,
  M_PRI,

  M_DATE,
  M_FULLDATE,
  M_ISODATE,
  M_STAMP,
  M_YEAR,
  M_MONTH,
  M_DAY,
  M_HOUR,
  M_MIN,
  M_SEC,
  M_WEEKDAY,
  M_WEEK,
  M_TZOFFSET,
  M_TZ,
  M_UNIXTIME,

  M_DATE_RECVD,
  M_FULLDATE_RECVD,
  M_ISODATE_RECVD,
  M_STAMP_RECVD,
  M_YEAR_RECVD,
  M_MONTH_RECVD,
  M_DAY_RECVD,
  M_HOUR_RECVD,
  M_MIN_RECVD,
  M_SEC_RECVD,
  M_WEEKDAY_RECVD,
  M_WEEK_RECVD,
  M_TZOFFSET_RECVD,
  M_TZ_RECVD,
  M_UNIXTIME_RECVD,

  M_DATE_STAMP,
  M_FULLDATE_STAMP,
  M_ISODATE_STAMP,
  M_STAMP_STAMP,
  M_YEAR_STAMP,
  M_MONTH_STAMP,
  M_DAY_STAMP,
  M_HOUR_STAMP,
  M_MIN_STAMP,
  M_SEC_STAMP,
  M_WEEKDAY_STAMP,
  M_WEEK_STAMP,
  M_TZOFFSET_STAMP,
  M_TZ_STAMP,
  M_UNIXTIME_STAMP,

  M_FULLHOST,
  M_HOST,
  M_FULLHOST_FROM,
  M_HOST_FROM,
  M_PROGRAM,
  M_PID,

  M_MESSAGE,
  M_MSGONLY,
  M_SOURCE_IP,
  M_MAX,

  M_MATCH_REF_OFS=256
};

#define M_TIME_MACROS 15

guint
log_macro_lookup(gchar *macro, gint len);

gboolean
log_macro_expand(GString *result, gint id, guint32 flags, gint ts_format, glong zone_offset, gint frac_digits, LogMessage *msg);

#endif
