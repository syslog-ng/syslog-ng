/*
 * Copyright (c) 2002-2011 Balabit
 * Copyright (c) 1998-2011 Balázs Scheidler
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

#include "block-ref-parser.h"
#include "block-ref-grammar.h"

extern int block_ref_debug;
int block_ref_parse(CfgLexer *lexer, CfgArgs **node, gpointer arg);

static CfgLexerKeyword block_ref_keywords[] =
{
  { CFG_KEYWORD_STOP },
};

CfgLexerKeyword *block_def_keywords = block_ref_keywords;

CfgParser block_ref_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &block_ref_debug,
#endif
  .name = "block reference",
  .context = LL_CONTEXT_BLOCK_REF,
  .keywords = block_ref_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer arg)) block_ref_parse,
  .cleanup = (void (*)(gpointer))cfg_args_unref
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(block_ref_, CfgArgs **)
