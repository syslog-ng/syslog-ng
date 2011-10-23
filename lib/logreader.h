/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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
  
#ifndef LOGREADER_H_INCLUDED
#define LOGREADER_H_INCLUDED

#include "logsource.h"
#include "logproto.h"
#include "timeutils.h"

/* flags */
#define LR_KERNEL          0x0002
#define LR_EMPTY_LINES     0x0004
#define LR_IGNORE_TIMEOUT  0x0008
#define LR_SYSLOG_PROTOCOL 0x0010
#define LR_PREEMPT         0x0020
#define LR_THREADED        0x0040

/* options */

typedef struct _LogReaderWatch LogReaderWatch;

typedef struct _LogReaderOptions
{
  LogSourceOptions super;
  MsgFormatOptions parse_options;
  guint32 flags;
  gint padding;
  gint msg_size;
  gint follow_freq;
  gint fetch_limit;
  gchar *text_encoding;
  const gchar *group_name;

  /* source time zone if one is not specified in the message */
  gboolean check_hostname;
} LogReaderOptions;

typedef struct _LogReader LogReader;

void log_reader_set_options(LogPipe *s, LogPipe *control, LogReaderOptions *options, gint stats_level, gint stats_source, const gchar *stats_id, const gchar *stats_instance);
void log_reader_set_follow_filename(LogPipe *self, const gchar *follow_filename);
void log_reader_set_peer_addr(LogPipe *s, GSockAddr *peer_addr);
void log_reader_set_immediate_check(LogPipe *s);

LogPipe *log_reader_new(LogProto *proto);
void log_reader_options_defaults(LogReaderOptions *options);
void log_reader_options_init(LogReaderOptions *options, GlobalConfig *cfg, const gchar *group_name);
void log_reader_options_destroy(LogReaderOptions *options);
gint log_reader_options_lookup_flag(const gchar *flag);
void log_reader_options_set_tags(LogReaderOptions *options, GList *tags);
gboolean log_reader_options_process_flag(LogReaderOptions *options, gchar *flag);

#endif
