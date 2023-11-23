/*
 * Copyright (c) 2002-2014 Balabit
 * Copyright (c) 1998-2014 Balázs Scheidler
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

#ifndef TEMPLATE_MACROS_H_INCLUDED
#define TEMPLATE_MACROS_H_INCLUDED

#include "syslog-ng.h"
#include "common-template-typedefs.h"
#include "eval.h"

/* macro IDs */
enum
{
  M_NONE,

  M_FACILITY,
  M_FACILITY_NUM,
  M_SEVERITY,
  M_SEVERITY_NUM,
  M_TAG,
  M_TAGS,
  M_BSDTAG,
  M_PRI,

  M_HOST,
  M_SDATA,

  M_MSGHDR,
  M_MESSAGE,
  M_SOURCE_IP,
  M_DEST_IP,
  M_DEST_PORT,
  M_IP_PROTOCOL,
  M_PROTOCOL,
  M_RAWMSG_SIZE,
  M_SEQNUM,
  M_CONTEXT_ID,

  M_LOGHOST,
  M_SYSUPTIME,
  M_RCPTID,
  M_RUNID,
  M_HOSTID,
  M_UNIQID,
  M__ASTERISK,

  /* only touch this section if you want to add three macros, one w/o
   * prefix, and a R_ and S_ prefixed macro that relates one of the
   * timestamps of the log message. */

  M_DATE,
  M_FULLDATE,
  M_ISODATE,
  M_STAMP,
  M_YEAR,
  M_YEAR_DAY,
  M_MONTH,
  M_MONTH_WEEK,
  M_MONTH_ABBREV,
  M_MONTH_NAME,
  M_DAY,
  M_HOUR,
  M_HOUR12,
  M_MIN,
  M_SEC,
  M_USEC,
  M_MSEC,
  M_AMPM,
  M_WEEK_DAY,
  M_WEEK_DAY_ABBREV,
  M_WEEK_DAY_NAME,
  M_WEEK,
  M_ISOWEEK,
  M_TZOFFSET,
  M_TZ,
  M_UNIXTIME,
  M_TIME_FIRST = M_DATE,
  M_TIME_LAST = M_UNIXTIME,
  M_TIME_MACROS_MAX = M_UNIXTIME - M_DATE + 1,

  M_RECVD_OFS = M_TIME_MACROS_MAX,
  M_STAMP_OFS = 2 * M_TIME_MACROS_MAX,
  M_CSTAMP_OFS = 3 * M_TIME_MACROS_MAX,
  M_PROCESSED_OFS = 4 * M_TIME_MACROS_MAX,
};

/* macros (not NV pairs!) that syslog-ng knows about. This was the
 * earliest mechanism for inserting message-specific information into
 * texts. It is now superseded by name-value pairs where the value is
 * text, but remains to be used for time and other metadata.
 */
typedef struct _LogMacroDef
{
  const char *name;
  int id;
} LogMacroDef;

extern LogMacroDef macros[];

/* low level macro functions */
guint log_macro_lookup(const gchar *macro, gint len);
gboolean log_macro_expand(gint id, LogTemplateEvalOptions *options,
                          const LogMessage *msg,
                          GString *result, LogMessageValueType *type);
gboolean log_macro_expand_simple(gint id, const LogMessage *msg,
                                 GString *result, LogMessageValueType *type);

void log_macros_global_init(void);
void log_macros_global_deinit(void);

#endif
