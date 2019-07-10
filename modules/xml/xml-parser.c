/*
 * Copyright (c) 2017 Balabit
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

#include "xml.h"
#include "cfg-parser.h"
#include "xml-grammar.h"

extern int xml_debug;

int xml_parse(CfgLexer *lexer, LogParser **instance, gpointer arg);

static CfgLexerKeyword xml_keywords[] =
{
  { "xml",          KW_XML },
  { "prefix",       KW_PREFIX },
  { "drop_invalid", KW_DROP_INVALID },
  { "exclude_tags", KW_EXCLUDE_TAGS },
  { "strip_whitespaces", KW_STRIP_WHITESPACES },
  { "create_lists", KW_CREATE_LISTS },
  { NULL }
};

CfgParser xml_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &xml_debug,
#endif
  .name = "xml",
  .keywords = xml_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) xml_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(xml_, LogParser **)
