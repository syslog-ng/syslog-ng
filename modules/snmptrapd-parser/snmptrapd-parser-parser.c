/*
 * Copyright (c) 2017 Balabit
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

#include "snmptrapd-parser.h"
#include "cfg-parser.h"
#include "snmptrapd-parser-grammar.h"

extern int snmptrapd_parser_debug;

int snmptrapd_parser_parse(CfgLexer *lexer, LogParser **instance, gpointer arg);

static CfgLexerKeyword snmptrapd_parser_keywords[] =
{
  { "snmptrapd_parser",  KW_SNMPTRAPD_PARSER },
  { "prefix",            KW_PREFIX },
  { "set_message_macro", KW_SET_MESSAGE_MACRO },
  { NULL }
};

CfgParser snmptrapd_parser_parser =
{
#if defined (ENABLE_DEBUG)
  .debug_flag = &snmptrapd_parser_debug,
#endif
  .name = "snmptrapd-parser",
  .keywords = snmptrapd_parser_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) snmptrapd_parser_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(snmptrapd_parser_, LogParser **)

