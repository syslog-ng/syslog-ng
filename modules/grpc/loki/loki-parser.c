/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2024 Attila Szakacs <attila.szakacs@axoflow.com>
 * Copyright (c) 2023 László Várady
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

#include "driver.h"
#include "cfg-parser.h"
#include "loki-grammar.h"

extern int loki_debug;

int loki_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword loki_keywords[] =
{
  { "loki", KW_LOKI },
  { "url", KW_URL },
  { "labels", KW_LABELS },
  { "timestamp", KW_TIMESTAMP },
  { "keep_alive", KW_KEEP_ALIVE },
  { "time", KW_TIME },
  { "timeout", KW_TIMEOUT },
  { "max_pings_without_data", KW_MAX_PINGS_WITHOUT_DATA },
  { "auth", KW_AUTH },
  { "insecure", KW_INSECURE },
  { "tls", KW_TLS },
  { "key_file", KW_KEY_FILE },
  { "cert_file", KW_CERT_FILE },
  { "ca_file", KW_CA_FILE },
  { "alts", KW_ALTS },
  { "target_service_accounts", KW_TARGET_SERVICE_ACCOUNTS },
  { "adc", KW_ADC },
  { "tenant_id", KW_TENANT_ID },
  { "channel_args", KW_CHANNEL_ARGS },
  { "headers", KW_HEADERS },
  { NULL }
};

CfgParser loki_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &loki_debug,
#endif
  .name = "loki",
  .keywords = loki_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) loki_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(loki_, LOKI_, LogDriver **)
