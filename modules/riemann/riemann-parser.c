/*
 * Copyright (c) 2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2013 Gergely Nagy <algernon@balabit.hu>
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

#include "riemann.h"
#include "cfg-parser.h"
#include "riemann-grammar.h"

extern int riemann_debug;
int riemann_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword riemann_keywords[] = {
  { "riemann",                  KW_RIEMANN },
  { "server",                   KW_SERVER },
  { "port",                     KW_PORT },

  { "host",                     KW_HOST },
  { "service",                  KW_SERVICE },
  { "state",                    KW_STATE },
  { "description",              KW_DESCRIPTION },
  { "metric",                   KW_METRIC },
  { "ttl",                      KW_TTL },
  { "attributes",               KW_ATTRIBUTES },

  { NULL }
};

CfgParser riemann_parser =
{
#if ENABLE_DEBUG
  .debug_flag = &riemann_debug,
#endif
  .name = "riemann",
  .keywords = riemann_keywords,
  .parse = (int (*)(CfgLexer *lexer, gpointer *instance, gpointer)) riemann_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(riemann_, LogDriver **)
