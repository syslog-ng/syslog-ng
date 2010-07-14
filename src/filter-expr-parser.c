/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#include "filter-expr-parser.h"
#include "filter.h"
#include "filter-expr-grammar.h"

extern int filter_expr_debug;
int filter_expr_parse(CfgLexer *lexer, FilterExprNode **node);

static CfgLexerKeyword filter_expr_keywords[] = {
  { "or",                 KW_OR },
  { "and",                KW_AND },
  { "not",                KW_NOT },
  { "level",              KW_LEVEL },
  { "priority",           KW_LEVEL },
  { "facility",           KW_FACILITY },
  { "program",		  KW_PROGRAM },
  { "host",               KW_HOST },
  { "message",            KW_MESSAGE },
  { "match",		  KW_MATCH },
  { "netmask",		  KW_NETMASK },
  { NULL }
};

CfgParser filter_expr_parser =
{
#if ENABLE_DEBUG
  .debug_flag = &filter_expr_debug,
#endif
  .name = "filter expression",
  .context = LL_CONTEXT_FILTER,
  .keywords = filter_expr_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *)) filter_expr_parse,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(filter_expr_, FilterExprNode **)
