/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2018 Mehul Prajapati <mehulprajapati2802@gmail.com>
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

#include "redisq.h"
#include "cfg-parser.h"
#include "redisq-grammar.h"

extern int redisq_debug;
int redisq_parse(CfgLexer *lexer, LogDriverPlugin **instance, gpointer arg);

static CfgLexerKeyword redisq_keywords[] =
{
  { "redis_queue",   KW_REDIS_QUEUE },
  { "host",          KW_HOST },
  { "port",          KW_PORT },
  { "auth",          KW_AUTH },
  { "key_prefix",    KW_KEY_PREFIX },
  { "conn_timeout",  KW_CONN_TIMEOUT },
  { NULL }
};

CfgParser redisq_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &redisq_debug,
#endif
  .name = "redis_queue",
  .keywords = redisq_keywords,
  .parse = (int (*)(CfgLexer *lexer, gpointer *instance, gpointer arg)) redisq_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,

};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(redisq_, LogDriverPlugin **)
