/*
 * Copyright (c) 2002-2017 Balabit
 * Copyright (c) 1998-2017 Balázs Scheidler
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

#include "tags-parser.h"
#include "cfg-parser.h"
#include "tags-parser-grammar.h"

extern int tags_parser_debug;

int tags_parser_parse(CfgLexer *lexer, LogParser **instance, gpointer arg);

static CfgLexerKeyword tags_parser_keywords[] =
{
  { "tags_parser",  KW_TAGS_PARSER },
  { NULL }
};

CfgParser tags_parser_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &tags_parser_debug,
#endif
  .name = "tags_parser",
  .keywords = tags_parser_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) tags_parser_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(tags_parser_, TAGS_PARSER_, LogParser **)
