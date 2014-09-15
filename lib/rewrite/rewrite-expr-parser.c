/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#include "rewrite/rewrite-expr-parser.h"
#include "rewrite/rewrite-expr.h"
#include "rewrite/rewrite-expr-grammar.h"

extern int rewrite_expr_debug;
int rewrite_expr_parse(CfgLexer *lexer, LogExprNode **node, gpointer arg);

static CfgLexerKeyword rewrite_expr_keywords[] = {
  { "set",                KW_SET, 0x0300 },
  { "subst",              KW_SUBST, 0x0300 },
  { "set_tag",            KW_SET_TAG, 0x0304 },
  { "clear_tag",          KW_CLEAR_TAG, 0x0304 },

  { "type",               KW_TYPE, 0x0300 },
  { "flags",              KW_FLAGS },
  { "condition",          KW_CONDITION, 0x0302 },
  { "groupset",           KW_GROUP_SET, 0x0306 },
  { NULL }
};

CfgParser rewrite_expr_parser =
{
#if ENABLE_DEBUG
  .debug_flag = &rewrite_expr_debug,
#endif
  .name = "rewrite expression",
  .context = LL_CONTEXT_REWRITE,
  .keywords = rewrite_expr_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) rewrite_expr_parse,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(rewrite_expr_, LogExprNode **)
