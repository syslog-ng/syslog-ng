/*
 * Copyright (c) 2002-2011 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2010-2011 Gergely Nagy <algernon@balabit.hu>
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

#include "afmongodb.h"
#include "cfg-parser.h"
#include "afmongodb-grammar.h"

extern int afmongodb_debug;
int afmongodb_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword afmongodb_keywords[] = {
  { "mongodb",			KW_MONGODB },
  { "host",			KW_HOST },
  { "port",			KW_PORT },
  { "database",			KW_DATABASE },
  { "collection",		KW_COLLECTION },
  { "keys",			KW_KEYS },
  { "values",			KW_VALUES },
  { "username",			KW_USERNAME },
  { "password",			KW_PASSWORD },
  { "log_fifo_size",		KW_LOG_FIFO_SIZE  },
  { NULL }
};

CfgParser afmongodb_parser =
{
#if ENABLE_DEBUG
  .debug_flag = &afmongodb_debug,
#endif
  .name = "afmongodb",
  .keywords = afmongodb_keywords,
  .parse = (int (*)(CfgLexer *lexer, gpointer *instance, gpointer)) afmongodb_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(afmongodb_, LogDriver **)
