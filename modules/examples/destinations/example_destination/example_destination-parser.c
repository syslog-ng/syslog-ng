/*
 * Copyright (c) 2020 Balabit
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

#include "driver.h"
#include "cfg-parser.h"
#include "example_destination-grammar.h"

extern int example_destination_debug;

int example_destination_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword example_destination_keywords[] =
{
  { "example_destination", KW_EXAMPLE_DESTINATION },
  { "filename", KW_FILENAME },
  { NULL }
};

CfgParser example_destination_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &example_destination_debug,
#endif
  .name = "example_destination",
  .keywords = example_destination_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) example_destination_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(example_destination_, LogDriver **)
