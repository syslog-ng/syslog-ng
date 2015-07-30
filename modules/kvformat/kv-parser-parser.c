/*
 * Copyright (c) 2015 BalaBit
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

#include "kv-parser.h"
#include "cfg-parser.h"
#include "kv-parser-grammar.h"

extern int kv_parser_debug;

int kv_parser_parse(CfgLexer *lexer, LogParser **instance, gpointer arg);

static CfgLexerKeyword kv_parser_keywords[] =
{
  { "kv_parser",          KW_KV_PARSER,  },
  { "prefix",             KW_PREFIX,  },
  { NULL }
};

CfgParser kv_parser_parser =
{
#if ENABLE_DEBUG
  .debug_flag = &kv_parser_debug,
#endif
  .name = "kv-parser",
  .keywords = kv_parser_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) kv_parser_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(kv_parser_, LogParser **)
