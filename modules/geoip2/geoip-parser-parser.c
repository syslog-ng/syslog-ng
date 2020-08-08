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

#include "geoip-parser.h"
#include "cfg-parser.h"
#include "geoip-parser-grammar.h"

extern int geoip2_parser_debug;

int geoip2_parser_parse(CfgLexer *lexer, LogParser **instance, gpointer arg);

static CfgLexerKeyword geoip2_parser_keywords[] =
{
  { "geoip2",         KW_GEOIP2 },
  { "database",       KW_DATABASE },
  { "prefix",         KW_PREFIX },
  { NULL }
};

CfgParser geoip2_parser_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &geoip2_parser_debug,
#endif
  .name = "geoip2-parser",
  .keywords = geoip2_parser_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) geoip2_parser_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(geoip2_parser_, GEOIP2_PARSER_, LogParser **)
