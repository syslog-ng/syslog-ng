/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
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

#include "afstreams.h"
#include "cfg-parser.h"
#include "afstreams-grammar.h"

extern int afstreams_debug;

int afstreams_parse(CfgLexer *lexer, LogDriver **instance);

static CfgLexerKeyword afstreams_keywords[] = {
#if ENABLE_SUN_STREAMS
  { "sun_stream",         KW_SUN_STREAMS },
  { "sun_streams",        KW_SUN_STREAMS },
#endif
  { "door",               KW_DOOR },
  { NULL }
};

CfgParser afstreams_parser =
{
#if ENABLE_DEBUG
  .debug_flag = &afstreams_debug,
#endif
  .name = "afstreams",
  .keywords = afstreams_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *)) afstreams_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(afstreams_, LogDriver **)
