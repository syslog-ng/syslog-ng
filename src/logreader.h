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

#ifndef LOGREADER_H_INCLUDED
#define LOGREADER_H_INCLUDED

#include "logsource.h"
#include "fdread.h"

/* flags */
#define LR_LOCAL     0x0001
#define LR_INTERNAL  0x0002
/* end-of-packet terminates log message (UDP sources) */
#define LR_PKTTERM   0x0004
/* issue a single read in a poll loop as /proc/kmsg does not support non-blocking mode */
#define LR_NOMREAD   0x0008
#define LR_FOLLOW    0x0010
#define LR_STRICT    0x0020

#define LR_COMPLETE_LINE 0x0100

/* options */
#define LRO_NOPARSE        0x0001
#define LRO_CHECK_HOSTNAME 0x0002
#define LRO_KERNEL         0x0004

typedef struct _LogReaderOptions
{
  LogSourceOptions source_opts;
  guint32 options;
  gint padding;
  gchar *prefix;
  gint msg_size;
  gint follow_freq;
  gint fetch_limit;
  
  /* source time zone if one is not specified in the message */
  glong zone_offset;
  gboolean keep_timestamp;
  regex_t *bad_hostname;
} LogReaderOptions;

typedef struct _LogReader
{
  LogSource super;
  gchar *buffer;
  FDRead *fd;
  GSource *source;
  gint ofs;
  guint32 flags;
  LogPipe *control;
  GSockAddr *prev_addr;
  LogReaderOptions *options;
} LogReader;

void log_reader_set_options(LogPipe *self, LogReaderOptions *options);

LogPipe *log_reader_new(FDRead *fd, guint32 flags, LogPipe *control, LogReaderOptions *options);
void log_reader_options_defaults(LogReaderOptions *options);
void log_reader_options_init(LogReaderOptions *options, GlobalConfig *cfg);


#endif
