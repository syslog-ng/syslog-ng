/*
 * Copyright (c) @YEAR_AND_AUTHOR@
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
#include "@PLUGIN_NAME@-grammar.h"

extern int @PLUGIN_NAME_US@_debug;

int @PLUGIN_NAME_US@_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword @PLUGIN_NAME_US@_keywords[] =
{
  { NULL }
};

CfgParser @PLUGIN_NAME_US@_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &@PLUGIN_NAME_US@_debug,
#endif
  .name = "@PLUGIN_NAME@",
  .keywords = @PLUGIN_NAME_US@_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) @PLUGIN_NAME_US@_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(@PLUGIN_NAME_US@_, LogDriver **)
