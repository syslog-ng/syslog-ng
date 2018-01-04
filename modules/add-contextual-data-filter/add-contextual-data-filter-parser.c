/*
 * Copyright (c) 2016 Balabit
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

#include "add-contextual-data-filter.h"
#include "cfg-parser.h"
#include "add-contextual-data-filter-grammar.h"
#include "logpipe.h"
#include "parser/parser-expr.h"
#include "syslog-ng-config.h"
#include <glib.h>

extern int add_contextual_data_selector_filter_debug;

int add_contextual_data_selector_filter_parse(CfgLexer *lexer, AddContextualDataSelector **instance,
                                              gpointer arg);

static CfgLexerKeyword add_contextual_data_keywords[] =
{
  {"filters", KW_FILTERS},
  {NULL}
};

CfgParser add_contextual_data_selector_filter_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &add_contextual_data_selector_filter_debug,
#endif
  .name = "add_contextual_data_selector_filter",
  .keywords = add_contextual_data_keywords,
  .parse =
  (gint(*)(CfgLexer *, gpointer *, gpointer)) add_contextual_data_selector_filter_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(add_contextual_data_selector_filter_, AddContextualDataSelector **)
