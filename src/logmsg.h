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
  
#ifndef LOG_H_INCLUDED
#define LOG_H_INCLUDED

#include "syslog-ng.h"
#include "gsockaddr.h"
#include "atomic.h"
#include "serialize.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <regex.h>

#define LP_NOPARSE         0x0001
#define LP_INTERNAL        0x0002
#define LP_LOCAL           0x0004
#define LP_CHECK_HOSTNAME  0x0008
#define LP_STRICT	   0x0010
#define LP_KERNEL          0x0020

#define LF_UNPARSED 0x0001
#define LF_INTERNAL 0x0002
#define LF_LOCAL    0x0004
#define LF_MARK     0x0008

/* timestamp formats */
#define TS_FMT_BSD   0
#define TS_FMT_ISO   1
#define TS_FMT_FULL  2
#define TS_FMT_UNIX  3

typedef struct _LogMessage LogMessage;

typedef void (*LMAckFunc)(LogMessage *lm, gpointer user_data);

typedef struct _LogStamp
{
  struct timeval time;
  /* zone offset in seconds, add this to UTC to get the time in local */
  glong zone_offset;
} LogStamp;

void log_stamp_format(LogStamp *stamp, GString *target, gint ts_format, glong zone_offset, gint frac_digits);

#define RE_MAX_MATCHES 256

struct _LogMessage
{
  GAtomicCounter ref_cnt;
  GAtomicCounter ack_cnt;
  guint8 flags;
  guint8 pri;
  /* meta information */
  guint8 num_re_matches;
  guint8 __pad1;
  gchar *source_group;
  GSockAddr *saddr;
  LMAckFunc ack_func;
  gpointer ack_userdata;
  
  /* message parts */
  LogStamp stamp;
  LogStamp recvd;
  GString date, host, host_from, program, msg;

  gchar **re_matches;
};

LogMessage *log_msg_ref(LogMessage *m);
void log_msg_unref(LogMessage *m);

gboolean log_msg_write(LogMessage *self, SerializeArchive *sa);
gboolean log_msg_read(LogMessage *self, SerializeArchive *sa);

LogMessage *log_msg_new(gchar *msg, gint length, GSockAddr *saddr, guint flags, regex_t *bad_hostname);
LogMessage *log_msg_new_mark(void);
LogMessage *log_msg_new_empty(void);

void log_msg_add_ack(LogMessage *msg, guint path_flags);
void log_msg_ack(LogMessage *msg, guint path_flags);
void log_msg_drop(LogMessage *msg, guint path_flags);
void log_msg_clear_matches(LogMessage *self);

#endif
