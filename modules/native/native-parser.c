/*
 * Copyright (c) 2015 BalaBit
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
#include "logpipe.h"
#include "parser.h"
#include "native-grammar.h"

extern int native_debug;

__attribute__((__visibility__("hidden"))) int native_parse(CfgLexer *lexer, LogParser **instance, gpointer arg);

static CfgLexerKeyword native_keywords[] = {
  { "option",   KW_OPTION },
  { NULL }
};
 
__attribute__((__visibility__("hidden"))) CfgParser native_parser =
{
#if ENABLE_DEBUG
  .debug_flag = &native_debug,
#endif
  .context = LL_IDENTIFIER,
  .name = "native-module",
  .keywords = native_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) native_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(native_, LogParser **)

