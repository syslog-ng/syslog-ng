/*
 * Copyright (c) 2014-2015 Balabit
 * Copyright (c) 2014 Gergely Nagy <algernon@balabit.hu>
 * Copyright (c) 2015 Balazs Scheidler <balazs.scheidler@balabit.com>
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

#include "python-parser.h"
#include "cfg-parser.h"
#include "python-grammar.h"

extern int python_debug;
int python_parse(CfgLexer *lexer, void **instance, gpointer arg);

static CfgLexerKeyword python_keywords[] =
{
  { "python",                   KW_PYTHON  },
  { "class",                    KW_CLASS   },
  {
    "imports",                  KW_IMPORTS, KWS_OBSOLETE,
    "imports() has been deprecated, please use loaders()"
  },
  { "loaders",                    KW_LOADERS   },
  { NULL }
};

CfgParser python_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &python_debug,
#endif
  .name = "python",
  .keywords = python_keywords,
  .parse = (int (*)(CfgLexer *lexer, gpointer *instance, gpointer)) python_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(python_, void **)
