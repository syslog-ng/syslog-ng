/*
 * Copyright (c) 2012 Balabit
 * Copyright (c) 2012 Balázs Scheidler
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

#include "syslog-parser.h"
#include "cfg-parser.h"
#include "syslog-parser-grammar.h"

extern int syslog_parser_debug;

int syslog_parser_parse(CfgLexer *lexer, LogParser **instance, gpointer arg);

static CfgLexerKeyword syslog_parser_keywords[] =
{
  { "syslog_parser",          KW_SYSLOG_PARSER },
  { "drop_invalid",           KW_DROP_INVALID },
  { NULL }
};

CfgParser syslog_parser_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &syslog_parser_debug,
#endif
  .name = "syslog_parser",
  .keywords = syslog_parser_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) syslog_parser_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(syslog_parser_, SYSLOG_PARSER_, LogParser **)
