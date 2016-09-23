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

#include "python-parser-parser.h"
#include "cfg-parser.h"
#include "python-parser-grammar.h"

extern int python_debug;
int python_parser_parse(CfgLexer *lexer, LogParser **instance, gpointer arg);

static CfgLexerKeyword python_keywords[] =
{
  { "python_parser",            KW_PYTHON_PARSER  },
  { "class",                    KW_CLASS   },
  { "imports",                  KW_IMPORTS },
  { "option",                   KW_OPTION  },
  { NULL }
};

CfgParser python_parser_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &python_debug,
#endif
  .name = "python-parser",
  .keywords = python_keywords,
  .parse = (int (*)(CfgLexer *lexer, gpointer *instance, gpointer)) python_parser_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(python_parser_, LogParser **)
