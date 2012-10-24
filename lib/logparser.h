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

#ifndef LOGPARSER_H_INCLUDED
#define LOGPARSER_H_INCLUDED

#include "logmsg.h"
#include "messages.h"
#include "logpipe.h"
#include "templates.h"

typedef struct _LogParser LogParser;
typedef struct _LogColumnParser LogColumnParser;

struct _LogParser
{
  LogPipe super;
  LogTemplate *template;
  gboolean (*process)(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input, gsize input_len);
  gchar *name;
};

void log_parser_set_template(LogParser *self, LogTemplate *template);
void log_parser_init_instance(LogParser *self);
void log_parser_free_method(LogPipe *self);

static inline gboolean
log_parser_process(LogParser *self, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input, gssize input_len)
{
  if (input_len < 0)
    input_len = strlen(input);
  return self->process(self, pmsg, path_options, input, input_len);
}

struct _LogColumnParser
{
  LogParser super;
  GList *columns;
};

void log_column_parser_set_columns(LogColumnParser *s, GList *fields);
void log_column_parser_init_instance(LogColumnParser *self);
void log_column_parser_free_method(LogPipe *s);

#endif

