/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Gergely Nagy
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

extern int geoip_parser_debug;

int geoip_parser_parse(CfgLexer *lexer, LogParser **instance, gpointer arg);

static CfgLexerKeyword geoip_parser_keywords[] =
{
  { "geoip",          KW_GEOIP },
  { "database",       KW_DATABASE },
  { "prefix",         KW_PREFIX },
  { NULL }
};

CfgParser geoip_parser_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &geoip_parser_debug,
#endif
  .name = "geoip-parser",
  .keywords = geoip_parser_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) geoip_parser_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(geoip_parser_, LogParser **)
