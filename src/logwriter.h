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

#ifndef LOG_WRITER_H_INCLUDED
#define LOG_WRITER_H_INCLUDED

#include "logpipe.h"
#include "fdwrite.h"
#include "templates.h"

/* writer constructor flags */
#define LW_DETECT_EOF    0x0001
#define LW_FORMAT_FILE   0x0002
#define LW_FORMAT_PROTO  0x0004

/* writer options (set by the user) */
#define LWO_TMPL_ESCAPE     0x0001

/* we don't want to have a dropped counter for this writer */
#define LWOF_NO_STATS        0x0001
/* several writers use the same counter */
#define LWOF_SHARE_STATS     0x0002

typedef struct _LogWriterOptions
{
  gchar *stats_name;
  /* bitmask of LWO_* (set by the user) */
  guint32 options;
  /* bitmask of LWOF_* (set by the driver) */
  guint32 flags;
  
  /* maximum number of entries */
  gint fifo_size;
  
  /* minimum number of entries to trigger a flush */
  gint flush_lines;
  
  /* flush anyway if this time was elapsed */
  gint flush_timeout;
  LogTemplate *template;
  LogTemplate *file_template;
  LogTemplate *proto_template;
  
  gboolean use_time_recvd; /* deprecated */
  gshort ts_format;
  glong zone_offset;
  gshort frac_digits;
} LogWriterOptions;

typedef struct _LogWriter
{
  LogPipe super;
  GSource *source;
  GQueue *queue;
  guint32 flags;
  guint32 *dropped_messages;
  GString *partial;
  gint partial_pos;
  LogPipe *control;
  LogWriterOptions *options;
} LogWriter;

LogPipe *log_writer_new(guint32 flags, LogPipe *control, LogWriterOptions *options);
gboolean log_writer_reopen(LogPipe *s, FDWrite *fd);

void log_writer_options_set_template_escape(LogWriterOptions *options, gboolean enable);
void log_writer_options_defaults(LogWriterOptions *options);
void log_writer_options_init(LogWriterOptions *options, GlobalConfig *cfg, guint32 flags, const gchar *stats_name);
void log_writer_options_destroy(LogWriterOptions *options);

#endif
