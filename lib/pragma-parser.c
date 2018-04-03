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

#include "pragma-parser.h"
#include "pragma-grammar.h"

extern int pragma_debug;
int pragma_parse(CfgLexer *lexer, gpointer *result, gpointer arg);

static CfgLexerKeyword pragma_keywords[] =
{
  { "version",            KW_VERSION, },
  { "verzió",             KW_VERSION, },
  { "include",            KW_INCLUDE, },
  { "module",             KW_MODULE, },
  { "define",             KW_DEFINE, },
  { "lang",               KW_LANG, },
  { "nyelv",              KW_LANG, },
  { CFG_KEYWORD_STOP },
};

CfgParser pragma_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &pragma_debug,
#endif
  .name = "pragma",
  .context = LL_CONTEXT_PRAGMA,
  .keywords = pragma_keywords,
  .parse = pragma_parse,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(pragma_, gpointer *)
