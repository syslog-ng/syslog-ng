/*
 * Copyright (c) 2022 One Identity
 * Copyright (c) 2022 Kokan
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

#include "flip-parser.h"
#include "cfg-parser.h"
#include "flip-parser-grammar.h"

extern int flip_parser_debug;

int flip_parser_parse(CfgLexer *lexer, LogParser **instance, gpointer arg);

static CfgLexerKeyword flip_parser_keywords[] =
{
  {"flip_parser", KW_FLIP_PARSER},
  {"flip_text", KW_FLIP_TEXT},
  {"reverse_text", KW_REVERSE_TEXT},
  {NULL}
};

CfgParser flip_parser_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &flip_parser_debug,
#endif
  .name = "flip-parser",
  .keywords = flip_parser_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) flip_parser_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(flip_parser_, FLIP_PARSER_, LogParser **)
