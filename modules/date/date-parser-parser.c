/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Vincent Bernat <Vincent.Bernat@exoscale.ch>
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

#include "date-parser.h"
#include "cfg-parser.h"
#include "date-grammar.h"
#include "date-parser-parser.h"

extern int date_debug;
int date_parse(CfgLexer *lexer, LogParser **instance, gpointer arg);

static CfgLexerKeyword date_keywords[] =
{
  { "date_parser", KW_DATE_PARSER },
  { "time_stamp",  KW_TIME_STAMP },
  { NULL }
};

CfgParser date_parser =
{
#if defined (ENABLE_DEBUG)
  .debug_flag = &date_debug,
#endif
  .name = "date-parser",
  .keywords = date_keywords,
  .parse = (int (*)(CfgLexer *, gpointer *, gpointer)) date_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(date_, LogParser **);
