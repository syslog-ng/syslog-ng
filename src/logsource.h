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

#ifndef LOGSOURCE_H_INCLUDED
#define LOGSOURCE_H_INCLUDED

#include "logpipe.h"

typedef struct _LogSourceOptions
{
  gint init_window_size;
  gint window_size;
} LogSourceOptions;

/**
 * LogSource:
 *
 * This structure encapsulates an object which generates messages without
 * defining how those messages are accepted by peers. The most prominent
 * derived class is LogReader which is an extended RFC3164 capable syslog
 * message processor used everywhere.
 **/
typedef struct _LogSource
{
  LogPipe super;
  LogSourceOptions *options;
} LogSource;

gboolean log_source_handle_line(LogSource *self, gchar *line, gint length, GSockAddr *saddr, guint parse_flags);

static inline gboolean
log_source_free_to_send(LogSource *self)
{
  return !!self->options->window_size;
}

void log_source_init_instance(LogSource *self, LogSourceOptions *options);
void log_source_options_defaults(LogSourceOptions *options);
void log_source_options_init(LogSourceOptions *options, GlobalConfig *cfg);



#endif
