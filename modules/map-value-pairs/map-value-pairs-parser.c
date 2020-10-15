/*
 * Copyright (c) 2017 Balabit
 * Copyright (c) 2017 Balazs Scheidler <balazs.scheidler@balabit.com>
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

#include "map-value-pairs-parser.h"
#include "cfg-parser.h"
#include "map-value-pairs-grammar.h"

extern int map_value_pairs_debug;
int map_value_pairs_parse(CfgLexer *lexer, LogParser **instance, gpointer arg);

static CfgLexerKeyword map_value_pairs_keywords[] =
{
  { "map_value_pairs",  KW_MAP_VALUE_PAIRS },
  { NULL }
};

CfgParser map_value_pairs_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &map_value_pairs_debug,
#endif
  .name = "map-value-pairs",
  .keywords = map_value_pairs_keywords,
  .parse = (int (*)(CfgLexer *lexer, gpointer *instance, gpointer)) map_value_pairs_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(map_value_pairs_, MAP_VALUE_PAIRS_, LogParser **)
