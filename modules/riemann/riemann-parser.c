/*
 * Copyright (c) 2013 Balabit
 * Copyright (c) 2013, 2015 Gergely Nagy
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

#include "riemann.h"
#include "cfg-parser.h"
#include "riemann-grammar.h"

extern int riemann_debug;
int riemann_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword riemann_keywords[] =
{
  { "riemann",                  KW_RIEMANN },
  { "server",                   KW_SERVER },
  { "port",                     KW_PORT },
  { "timeout",                  KW_TIMEOUT },
  { "host",                     KW_HOST },
  { "service",                  KW_SERVICE },
  { "event_time",               KW_EVENT_TIME },
  { "state",                    KW_STATE },
  { "description",              KW_DESCRIPTION },
  { "metric",                   KW_METRIC },
  { "ttl",                      KW_TTL },
  { "attributes",               KW_ATTRIBUTES },

  { "ca_file",                  KW_CA_FILE },
  { "cert_file",                KW_CERT_FILE },
  { "key_file",                 KW_KEY_FILE },

  /* compatibility with original but inconsistent option naming */
  { "cacert",                   KW_CA_FILE, KWS_OBSOLETE, "The cacert() option is deprecated in favour of ca-file()" },
  { "cert",                     KW_CERT_FILE, KWS_OBSOLETE, "The cert() option is deprecated in favour of cert-file()" },

  { "tls",                      KW_TLS },

  { NULL }
};

CfgParser riemann_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &riemann_debug,
#endif
  .name = "riemann",
  .keywords = riemann_keywords,
  .parse = (int (*)(CfgLexer *lexer, gpointer *instance, gpointer)) riemann_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(riemann_, LogDriver **)
