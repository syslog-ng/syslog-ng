/*
 * Copyright (c) 2012 Nagy, Attila <bra@fsn.hu>
 * Copyright (c) 2013 Balabit
 * Copyright (c) 2013 Viktor Tusa <tusa@balabit.hu>
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

#include "afstomp.h"
#include "cfg-parser.h"
#include "afstomp-grammar.h"

extern int afstomp_debug;
int afstomp_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword afstomp_keywords[] =
{
  { "stomp",      KW_STOMP },
  { "host",     KW_HOST },
  { "port",     KW_PORT },
  { "destination",    KW_STOMP_DESTINATION },
  { "persistent",   KW_PERSISTENT },
  { "ack",      KW_ACK },
  { "username",     KW_USERNAME },
  { "password",     KW_PASSWORD },
  { "body",     KW_BODY },
  { NULL }
};

CfgParser afstomp_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &afstomp_debug,
#endif
  .name = "afstomp",
  .keywords = afstomp_keywords,
  .parse = (int (*)(CfgLexer *lexer, gpointer *instance, gpointer)) afstomp_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(afstomp_, LogDriver **)
