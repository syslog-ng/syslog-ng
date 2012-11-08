/*
 * Copyright (c) 2002-2011 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2011 Balázs Scheidler
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

#include "dbparser.h"
#include "cfg-parser.h"
#include "dbparser-grammar.h"

extern int dbparser_debug;

int dbparser_parse(CfgLexer *lexer, LogParser **instance, gpointer arg);

static CfgLexerKeyword dbparser_keywords[] =
{
  { "db_parser",          KW_DB_PARSER, 0x0300 },
  { "inject_mode",        KW_INJECT_MODE, 0x0303 },
  { NULL }
};

CfgParser dbparser_parser =
{
#if ENABLE_DEBUG
  .debug_flag = &dbparser_debug,
#endif
  .name = "dbparser",
  .keywords = dbparser_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) dbparser_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(dbparser_, LogParser **)
