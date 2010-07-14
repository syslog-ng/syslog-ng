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

#include "rewrite-expr-parser.h"
#include "logrewrite.h"
#include "rewrite-expr-grammar.h"

extern int rewrite_expr_debug;
int rewrite_expr_parse(CfgLexer *lexer, GList **node);

static CfgLexerKeyword rewrite_expr_keywords[] = {
  { "set",                KW_SET, 0x0300 },
  { "subst",              KW_SUBST, 0x0300 },

  { "type",               KW_TYPE, 0x0300 },
  { "flags",              KW_FLAGS },
  { NULL }
};

CfgParser rewrite_expr_parser =
{
  .debug_flag = &rewrite_expr_debug,
  .name = "rewrite expression",
  .context = LL_CONTEXT_REWRITE,
  .keywords = rewrite_expr_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *)) rewrite_expr_parse,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(rewrite_expr_, GList **)
