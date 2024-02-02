/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#ifndef LOG_WRITER_H_INCLUDED
#define LOG_WRITER_H_INCLUDED

#include "logpipe.h"
#include "template/templates.h"
#include "logqueue.h"
#include "logproto/logproto-client.h"
#include "stats/stats-cluster-key-builder.h"

/* writer constructor flags */
#define LW_DETECT_EOF        0x0001
#define LW_FORMAT_FILE       0x0002
#define LW_FORMAT_PROTO      0x0004
#define LW_SYSLOG_PROTOCOL   0x0008
#define LW_SOFT_FLOW_CONTROL 0x0010

/* writer options (set by the user) */
#define LWO_SYSLOG_PROTOCOL   0x0001
#define LWO_NO_MULTI_LINE     0x0002
/* we don't want to have a dropped counter for this writer */
#define LWO_NO_STATS        0x0004
#define LWO_THREADED        0x0010
#define LWO_IGNORE_ERRORS   0x0020
#define LWO_SEQNUM_ALL      0x0040
#define LWO_SEQNUM          0x0080

typedef struct _LogWriterOptions
{
  gboolean initialized;
  /* bitmask of LWO_* */
  guint32 options;

  /* minimum number of entries to trigger a flush */
  gint flush_lines;

  LogTemplate *template;
  LogTemplate *file_template;
  LogTemplate *proto_template;

  LogTemplateOptions template_options;
  HostResolveOptions host_resolve_options;
  LogProtoClientOptionsStorage proto_options;

  gint time_reopen;
  gint suppress;
  gint padding;
  gint mark_mode;
  gint mark_freq;
  gint stats_level;
  gint stats_source;
  gint truncate_size;
} LogWriterOptions;

typedef struct _LogWriter LogWriter;

void log_writer_set_flags(LogWriter *self, guint32 flags);
guint32 log_writer_get_flags(LogWriter *self);
void log_writer_set_options(LogWriter *self, LogPipe *control, LogWriterOptions *options, const gchar *stats_id,
                            StatsClusterKeyBuilder *kb);
void log_writer_format_log(LogWriter *self, LogMessage *lm, GString *result);
gboolean log_writer_has_pending_writes(LogWriter *self);
gboolean log_writer_opened(LogWriter *self);
void log_writer_reopen(LogWriter *self, LogProtoClient *proto);
LogProtoClient *log_writer_steal_proto(LogWriter *self);
void log_writer_set_queue(LogWriter *self, LogQueue *queue);
LogQueue *log_writer_get_queue(LogWriter *s);
LogWriter *log_writer_new(guint32 flags, GlobalConfig *cfg);
void log_writer_msg_rewind(LogWriter *self);

void log_writer_options_set_template_escape(LogWriterOptions *options, gboolean enable);
void log_writer_options_defaults(LogWriterOptions *options);
void log_writer_options_init(LogWriterOptions *options, GlobalConfig *cfg, guint32 option_flags);
void log_writer_options_destroy(LogWriterOptions *options);
void log_writer_options_set_mark_mode(LogWriterOptions *options, const gchar *mark_mode);
gboolean log_writer_options_process_flag(LogWriterOptions *options, const gchar *flag);

#endif
