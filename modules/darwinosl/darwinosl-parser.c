/*
 * Copyright (c) 2023 Hofi <hofione@gmail.com>
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
#include "darwinosl-source.h"
#include "darwinosl-grammar.h"


int darwinosl_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword darwinosl_keywords[] =
{
  { "darwinosl",              KW_DARWIN_OSL },
  { "filter_predicate",       KW_FILTER_PREDICATE },
  { "go_reverse",             KW_GO_REVERSE },
  { "do_not_use_bookmark",    KW_DO_NOT_USE_BOOKMARK },
  { "max_bookmark_distance",  KW_MAX_BOOKMARK_DISTANCE },
  { "fetch_delay",            KW_FETCH_DELAY },
  { "fetch_retry_delay",      KW_FETCH_RETRY_DELAY },

  { NULL }
};

CfgParser darwinosl_parser =
{
  .name = "darwin-oslogger",
  .keywords = darwinosl_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer arg)) darwinosl_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(darwinosl_, DARWIN_OSL_, LogDriver **)
