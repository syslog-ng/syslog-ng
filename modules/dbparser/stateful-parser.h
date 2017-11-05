/*
 * Copyright (c) 2002-2013, 2015 Balabit
 * Copyright (c) 1998-2013, 2015 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#ifndef STATEFUL_PARSER_H_INCLUDED
#define STATEFUL_PARSER_H_INCLUDED 1

#include "parser/parser-expr.h"

typedef enum
{
  LDBP_IM_PASSTHROUGH = 0,
  LDBP_IM_INTERNAL = 1,
} LogDBParserInjectMode;

typedef struct _StatefulParser
{
  LogParser super;
  LogDBParserInjectMode inject_mode;
} StatefulParser;

static inline gboolean
stateful_parser_init_method(LogPipe *s)
{
  return log_parser_init_method(s);
}

static inline gboolean
stateful_parser_deinit_method(LogPipe *s)
{
  return log_parser_deinit_method(s);
}

void stateful_parser_set_inject_mode(StatefulParser *self, LogDBParserInjectMode inject_mode);
void stateful_parser_emit_synthetic(StatefulParser *self, LogMessage *msg);
void stateful_parser_init_instance(StatefulParser *self, GlobalConfig *cfg);
void stateful_parser_free_method(LogPipe *s);

int stateful_parser_lookup_inject_mode(const gchar *inject_mode);

#endif
