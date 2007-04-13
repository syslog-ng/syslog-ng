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

/* flags */
#define LW_DETECT_EOF    0x0001
#define LW_FORMAT_FILE   0x0002
#define LW_FORMAT_PROTO  0x0004

/* writer options */
#define LWO_TMPL_ESCAPE     0x0001
#define LWO_TMPL_TIME_RECVD 0x0002

typedef struct _LogWriterOptions
{
  guint32 options;
  gint fifo_size;
  LogTemplate *template;
  LogTemplate *file_template;
  LogTemplate *proto_template;
  
  gint keep_timestamp;
  gint use_time_recvd;
  gint tz_convert;
  gint ts_format;
} LogWriterOptions;

typedef struct _LogWriter
{
  LogPipe super;
  GSource *source;
  GQueue *queue;
  guint32 flags;
  GString *partial;
  gint partial_pos;
  LogPipe *control;
  LogWriterOptions *options;
} LogWriter;

LogPipe *log_writer_new(guint32 flags, LogPipe *control, LogWriterOptions *options);
gboolean log_writer_reopen(LogPipe *s, FDWrite *fd);
void log_writer_options_defaults(LogWriterOptions *options);
void log_writer_options_init(LogWriterOptions *options, GlobalConfig *cfg, gboolean fixed_stamps);

#endif
