/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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
 *
 */

#include "csvparser.h"
#include "cfg-parser.h"
#include "csvparser-grammar.h"

extern int csvparser_debug;

int csvparser_parse(CfgLexer *lexer, LogParser **instance, gpointer arg);

static CfgLexerKeyword csvparser_keywords[] =
{
  { "csv_parser",          KW_CSV_PARSER, 0x0300 },
  { NULL }
};

CfgParser csvparser_parser =
{
#if ENABLE_DEBUG
  .debug_flag = &csvparser_debug,
#endif
  .name = "csvparser",
  .keywords = csvparser_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) csvparser_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(csvparser_, LogParser **)
