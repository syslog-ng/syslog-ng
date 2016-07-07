/*
 * Copyright (c) 2016 Balabit
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

#include "kvtagger.h"
#include "cfg-lexer.h"
#include "cfg-parser.h"
#include "kvtagger-grammar.h"
#include "logpipe.h"
#include "parser/parser-expr.h"
#include "syslog-ng-config.h"
#include <glib.h>

extern int kvtagger_debug;

int kvtagger_parse(CfgLexer *lexer, LogParser **instance, gpointer arg);

static CfgLexerKeyword kvtagger_keywords[] =
{
  { "kvtagger", KW_KVTAGGER},
  { "database", KW_KVTAGGER_DATABASE },
  { "selector", KW_KVTAGGER_SELECTOR },
  { "default_selector", KW_KVTAGGER_DEFAULT_SELECTOR },
  { "prefix", KW_KVTAGGER_PREFIX },
  { NULL }
};

CfgParser kvtagger_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &kvtagger_debug,
#endif
  .name = "kvtagger",
  .keywords = kvtagger_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) kvtagger_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(kvtagger_, LogParser **)
