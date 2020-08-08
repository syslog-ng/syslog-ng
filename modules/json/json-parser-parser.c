/*
 * Copyright (c) 2011-2012 Balabit
 * Copyright (c) 2011-2012 Gergely Nagy <algernon@balabit.hu>
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
 */

#include "json-parser.h"
#include "cfg-parser.h"
#include "json-parser-grammar.h"

extern int json_parser_debug;

int json_parser_parse(CfgLexer *lexer, LogParser **instance, gpointer arg);

static CfgLexerKeyword json_parser_keywords[] =
{
  { "json_parser",          KW_JSON_PARSER,  },
  { "prefix",               KW_PREFIX,  },
  { "marker",               KW_MARKER,  },
  { "extract_prefix",       KW_EXTRACT_PREFIX, },
  { NULL }
};

CfgParser json_parser_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &json_parser_debug,
#endif
  .name = "json-parser",
  .keywords = json_parser_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) json_parser_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(json_parser_, JSON_PARSER_, LogParser **)
