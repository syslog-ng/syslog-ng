/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
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
  
#ifndef LOGREADER_H_INCLUDED
#define LOGREADER_H_INCLUDED

#include "logsource.h"
#include "logproto.h"
#include "timeutils.h"

/* flags */
#define LR_LOCAL          0x0001
#define LR_INTERNAL       0x0002
#define LR_FOLLOW         0x0010
#define LR_STRICT         0x0020
#define LR_IGNORE_TIMEOUT 0x0040
#define LR_SYSLOG_PROTOCOL 0x0080
#define LR_PREEMPT         0x0100

/* options */
#define LRO_NOPARSE          0x0001
#define LRO_KERNEL           0x0002
#define LRO_SYSLOG_PROTOCOL  0x0004
#define LRO_VALIDATE_UTF8    0x0008
#define LRO_NO_MULTI_LINE    0x0010
#define LRO_DONT_STORE_LEGACY_MSGHDR 0x0020
#define LRO_EMPTY_LINES      0x0040

typedef struct _LogReaderWatch LogReaderWatch;

typedef struct _LogReaderOptions
{
  LogSourceOptions super;
  guint32 options;
  gint padding;
  gint msg_size;
  gint follow_freq;
  gint fetch_limit;
  gchar *text_encoding;
  const gchar *group_name;
  gint default_pri;

  /* source time zone if one is not specified in the message */
  gboolean check_hostname;
  gchar *recv_time_zone;
  TimeZoneInfo *recv_time_zone_info;
  regex_t *bad_hostname;
  GArray *tags;
} LogReaderOptions;

typedef struct _LogReader
{
  LogSource super;
  LogProto *proto;
  LogReaderWatch *source;
  guint32 flags;
  gboolean immediate_check;
  gboolean waiting_for_preemption;
  LogPipe *control;
  LogReaderOptions *options;
  GSockAddr *peer_addr;
  gchar *follow_filename;
  ino_t inode;
  gint64 size;
  
} LogReader;

void log_reader_set_options(LogPipe *s, LogPipe *control, LogReaderOptions *options, gint stats_level, gint stats_source, const gchar *stats_id, const gchar *stats_instance);
void log_reader_set_follow_filename(LogPipe *self, const gchar *follow_filename);
void log_reader_set_peer_addr(LogPipe *s, GSockAddr *peer_addr);
void log_reader_set_immediate_check(LogPipe *s);

void log_reader_update_pos(LogReader *self, gint64 ofs);
void log_reader_save_state(LogReader *self, SerializeArchive *archive);
void log_reader_restore_state(LogReader *self, SerializeArchive *archive);

LogPipe *log_reader_new(LogProto *proto, guint32 flags);
void log_reader_options_defaults(LogReaderOptions *options);
void log_reader_options_init(LogReaderOptions *options, GlobalConfig *cfg, const gchar *group_name);
void log_reader_options_destroy(LogReaderOptions *options);
gint log_reader_options_lookup_flag(const gchar *flag);
void log_reader_options_set_tags(LogReaderOptions *options, GList *tags);


#endif
