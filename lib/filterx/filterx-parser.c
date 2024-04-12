/*
 * Copyright (c) 2023 Balazs Scheidler <balazs.scheidler@axoflow.com>
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

#include "filterx/filterx-parser.h"
#include "filterx/filterx-grammar.h"

extern int filterx_debug;
int filterx_parse(CfgLexer *lexer, GList **node, gpointer arg);

static CfgLexerKeyword filterx_keywords[] =
{
  { "or",                 KW_OR },
  { "and",                KW_AND },
  { "not",                KW_NOT },
  { "lt",                 KW_STR_LT },
  { "le",                 KW_STR_LE },
  { "eq",                 KW_STR_EQ },
  { "ne",                 KW_STR_NE },
  { "ge",                 KW_STR_GE },
  { "gt",                 KW_STR_GT },

  { "true",               KW_TRUE },
  { "false",              KW_FALSE },
  { "null",               KW_NULL },
  { "enum",               KW_ENUM },

  { "if",                 KW_IF },
  { "else",               KW_ELSE },
  { "elif",               KW_ELIF },

  { "isset",              KW_ISSET },
  { "unset",              KW_UNSET },
  { "declare",            KW_DECLARE },

  /* TODO: This should be done via generator function. */
  { "regexp_search",      KW_REGEXP_SEARCH },

  { CFG_KEYWORD_STOP },
};

CfgParser filterx_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &filterx_debug,
#endif
  .name = "filterx expression",
  .context = LL_CONTEXT_FILTERX,
  .keywords = filterx_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) filterx_parse,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(filterx_, FILTERX_, GList **)
