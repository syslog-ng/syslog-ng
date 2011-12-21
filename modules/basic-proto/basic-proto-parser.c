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

#include "basic-proto.h"
#include "cfg-parser.h"
#include "driver.h"
#include "basic-proto-grammar.h"

extern int basic_proto_debug;

int basic_proto_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword basic_proto_keywords[] = {
/*  { "unix_dgram",	KW_UNIX_DGRAM },
  { "unix_stream",	KW_UNIX_STREAM },
  { "udp",                KW_UDP },
  { "tcp",                KW_TCP },
  { "syslog",             KW_SYSLOG },
#if ENABLE_IPV6
  { "udp6",               KW_UDP6 },
  { "tcp6",               KW_TCP6 },
#endif
*/
};

CfgParser basic_proto_parser =
{
#if ENABLE_DEBUG
  .debug_flag = &basic_proto_debug,
#endif
  .name = "basic-proto",
  .keywords = basic_proto_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) basic_proto_parse,
  .cleanup = (void (*)(gpointer)) log_proto_free,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(basic_proto_, LogDriver **)
