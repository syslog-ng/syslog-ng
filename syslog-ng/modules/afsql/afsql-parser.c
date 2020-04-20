/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#include "afsql.h"
#include "cfg-parser.h"
#include "afsql-grammar.h"

extern int afsql_debug;

int afsql_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword afsql_keywords[] =
{
  { "sql",                KW_SQL },
  { "username",           KW_USERNAME },
  { "password",           KW_PASSWORD },
  { "database",           KW_DATABASE },
  { "table",              KW_TABLE },

  { "columns",            KW_COLUMNS },
  { "indexes",            KW_INDEXES },
  { "values",             KW_VALUES },
  { "frac_digits",        KW_FRAC_DIGITS },
  { "session_statements", KW_SESSION_STATEMENTS },
  { "host",               KW_HOST },
  { "port",               KW_PORT },
  { "default",            KW_DEFAULT },

  { "local_time_zone",    KW_LOCAL_TIME_ZONE },
  { "null",               KW_NULL },
  { "retry_sql_inserts",  KW_RETRIES },
  { "flush_lines",        KW_BATCH_LINES, KWS_OBSOLETE, "The flush-lines() option is deprecated, use batch-lines() instead." },
  { "flush_timeout",      KW_BATCH_TIMEOUT, KWS_OBSOLETE, "The flush-timeout() option is deprecated, use batch-timeout() instead." },
  { "flags",              KW_FLAGS },
  { "create_statement_append", KW_CREATE_STATEMENT_APPEND },
  { "ignore_tns_config",  KW_IGNORE_TNS_CONFIG },

  { "dbd_option",         KW_DBD_OPTION },
  { NULL }
};

CfgParser afsql_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &afsql_debug,
#endif
  .name = "afsql",
  .keywords = afsql_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) afsql_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(afsql_, LogDriver **)
