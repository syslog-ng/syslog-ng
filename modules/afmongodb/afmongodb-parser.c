/*
 * Copyright (c) 2010-2015 Balabit
 * Copyright (c) 2010-2014 Gergely Nagy <algernon@balabit.hu>
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

#include "afmongodb.h"
#include "cfg-parser.h"
#include "afmongodb-grammar.h"

extern int afmongodb_debug;
int afmongodb_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword afmongodb_keywords[] = {
  { "mongodb", KW_MONGODB },
  { "uri", KW_URI },
  { "collection", KW_COLLECTION },
#if SYSLOG_NG_ENABLE_LEGACY_MONGODB_OPTIONS
  { "servers", KW_SERVERS, KWS_OBSOLETE, "Use the uri() option instead of servers()" },
  { "database", KW_DATABASE, KWS_OBSOLETE, "Use the uri() option instead of database()" },
  { "username", KW_USERNAME, KWS_OBSOLETE,
    "Use the uri() option instead of username() and password()" },
  { "password", KW_PASSWORD, KWS_OBSOLETE,
    "Use the uri() option instead of username() and password()" },
  { "safe_mode", KW_SAFE_MODE, KWS_OBSOLETE, "Use the uri() option instead of safe_mode()" },
  { "host", KW_HOST, KWS_OBSOLETE, "Use the uri() option instead of host() and port()" },
  { "port", KW_PORT, KWS_OBSOLETE, "Use the uri() option instead of host() and port()" },
  { "path", KW_PATH, KWS_OBSOLETE, "Use the uri() option instead of path()" },
#endif
  { }
};

CfgParser afmongodb_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &afmongodb_debug,
#endif
  .name = "afmongodb",
  .keywords = afmongodb_keywords,
  .parse = (int (*)(CfgLexer *lexer, gpointer *instance, gpointer)) afmongodb_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(afmongodb_, LogDriver **)
