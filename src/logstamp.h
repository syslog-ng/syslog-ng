/*
 * Copyright (c) 2002-2008 BalaBit IT Ltd, Budapest, Hungary
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
#ifndef LOGSTAMP_H_INCLUDED
#define LOGSTAMP_H_INCLUDED

#include "syslog-ng.h"

/* timestamp formats */
#define TS_FMT_BSD   0
#define TS_FMT_ISO   1
#define TS_FMT_FULL  2
#define TS_FMT_UNIX  3


typedef struct _LogStamp
{
  GTimeVal time;
  /* zone offset in seconds, add this to UTC to get the time in local */
  gint zone_offset;
} LogStamp;

void log_stamp_format(LogStamp *stamp, GString *target, gint ts_format, glong zone_offset, gint frac_digits);
void log_stamp_append_format(LogStamp *stamp, GString *target, gint ts_format, glong zone_offset, gint frac_digits);


#endif
