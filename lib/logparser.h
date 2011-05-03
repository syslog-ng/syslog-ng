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
#include "logprocess.h"
#include "templates.h"

typedef struct _LogParser LogParser;
typedef struct _LogColumnParser LogColumnParser;

struct _LogParser
{
  struct _LogTemplate *template;

  /* NOTE: init/deinit methods are added because db-parser() needs them, which now
   * requires adding init/deinit methods into 3 layers (LogProcessRule,
   * LogParser & LogDBParser).  The infrastructure in OSE 3.3 is much
   * better in this regard (LogParsers are LogPipes too, thus they have
   * their own init/deinit methods), so it is just a temporary solution for
   * 3.2.*/
  gboolean (*init)(LogParser *s, GlobalConfig *cfg);
  void (*deinit)(LogParser *s, GlobalConfig *cfg);
  gboolean (*process)(LogParser *s, LogMessage *msg, const gchar *input);
  void (*free_fn)(LogParser *s);
};

static inline gboolean
log_parser_init(LogParser *s, GlobalConfig *cfg)
{
  if (s->init)
    return s->init(s, cfg);
  return TRUE;
}

static inline void
log_parser_deinit(LogParser *s, GlobalConfig *cfg)
{
  if (s->deinit)
    s->deinit(s, cfg);
}

void log_parser_set_template(LogParser *self, LogTemplate *template);
gboolean log_parser_process(LogParser *self, LogMessage *msg);
void log_parser_free_method(LogParser *s);

static inline void
log_parser_free(LogParser *self)
{
  self->free_fn(self);
  g_free(self);
}

struct _LogColumnParser
{
  LogParser super;
  GList *columns;
};

void log_column_parser_set_columns(LogColumnParser *s, GList *fields);
void log_column_parser_free_method(LogParser *s);

LogProcessRule *log_parser_rule_new(const gchar *name, GList *parser_list);

#endif

