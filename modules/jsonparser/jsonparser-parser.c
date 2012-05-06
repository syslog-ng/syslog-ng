/*
 * Copyright (c) 2011 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2011 Gergely Nagy <algernon@balabit.hu>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
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
 */

#include "jsonparser.h"
#include "cfg-parser.h"
#include "jsonparser-grammar.h"

extern int jsonparser_debug;

int jsonparser_parse(CfgLexer *lexer, LogParser **instance, gpointer arg);

static CfgLexerKeyword jsonparser_keywords[] =
{
  { "json_parser",          KW_JSON_PARSER,  },
  { "prefix",               KW_PREFIX,  },
  { "marker",               KW_MARKER,  },
  { NULL }
};

CfgParser jsonparser_parser =
{
#if ENABLE_DEBUG
  .debug_flag = &jsonparser_debug,
#endif
  .name = "jsonparser",
  .keywords = jsonparser_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) jsonparser_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(jsonparser_, LogParser **)
