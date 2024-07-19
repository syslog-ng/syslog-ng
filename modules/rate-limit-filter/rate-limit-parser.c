/*
 * Copyright (c) 2021 One Identity
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2024 Attila Szakacs <attila.szakacs@axoflow.com>
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

#include "cfg-parser.h"
#include "filter/filter-expr.h"
#include "rate-limit-grammar.h"

extern int rate_limit_filter_debug;

int rate_limit_filter_parse(CfgLexer *lexer, FilterExprNode **instance, gpointer arg);

static CfgLexerKeyword rate_limit_filter_keywords[] =
{
  { "throttle", KW_THROTTLE },
  { "rate_limit", KW_RATE_LIMIT },
  { "rate", KW_RATE },
  { "template", KW_KEY, KWS_OBSOLETE, "The template() option is deprecated in favour of key()" },
  { "key", KW_KEY },
  { NULL }
};

CfgParser rate_limit_filter_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &rate_limit_filter_debug,
#endif
  .name = "rate-limit-filter",
  .keywords = rate_limit_filter_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) rate_limit_filter_parse,
  .cleanup = (void (*)(gpointer)) filter_expr_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(rate_limit_filter_, RATE_LIMIT_FILTER_, FilterExprNode **)
