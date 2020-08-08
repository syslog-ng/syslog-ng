/*
 * Copyright (c) 2011-2014 Balabit
 * Copyright (c) 2011-2014 Gergely Nagy <algernon@balabit.hu>
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

#include "afsmtp.h"
#include "cfg-parser.h"
#include "afsmtp-grammar.h"

extern int afsmtp_debug;
int afsmtp_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword afsmtp_keywords[] =
{
  { "smtp",     KW_SMTP },
  { "host",     KW_HOST },
  { "port",     KW_PORT },
  { "subject",      KW_SUBJECT },
  { "from",     KW_FROM },
  { "to",     KW_TO },
  { "cc",     KW_CC },
  { "bcc",      KW_BCC },
  { "reply_to",     KW_REPLY_TO },
  { "sender",     KW_SENDER },
  { "body",     KW_BODY },
  { "header",     KW_HEADER },
  { NULL }
};

CfgParser afsmtp_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &afsmtp_debug,
#endif
  .name = "afsmtp",
  .keywords = afsmtp_keywords,
  .parse = (int (*)(CfgLexer *lexer, gpointer *instance, gpointer)) afsmtp_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(afsmtp_, AFSMTP_, LogDriver **)
