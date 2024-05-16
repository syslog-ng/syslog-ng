/*
 * Copyright (c) 2023 Attila Szakacs
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
#include "otel-grammar.h"

extern int otel_debug;

int otel_parse(CfgLexer *lexer, void **instance, gpointer arg);

static CfgLexerKeyword otel_keywords[] =
{
  { "opentelemetry",             KW_OPENTELEMETRY },
  { "port",                      KW_PORT },
  { "auth",                      KW_AUTH },
  { "insecure",                  KW_INSECURE },
  { "tls",                       KW_TLS },
  { "key_file",                  KW_KEY_FILE },
  { "cert_file",                 KW_CERT_FILE },
  { "ca_file",                   KW_CA_FILE },
  { "peer_verify",               KW_PEER_VERIFY },
  { "optional_untrusted",        KW_OPTIONAL_UNTRUSTED },
  { "optional_trusted",          KW_OPTIONAL_TRUSTED },
  { "required_untrusted",        KW_REQUIRED_UNTRUSTED },
  { "required_trusted",          KW_REQUIRED_TRUSTED },
  { "alts",                      KW_ALTS },
  { "url",                       KW_URL },
  { "target_service_accounts",   KW_TARGET_SERVICE_ACCOUNTS },
  { "adc",                       KW_ADC },
  { "syslog_ng_otlp",            KW_SYSLOG_NG_OTLP },
  { "compression",               KW_COMPRESSION },
  { "batch_bytes",               KW_BATCH_BYTES },
  { "concurrent_requests",       KW_CONCURRENT_REQUESTS },
  { "channel_args",              KW_CHANNEL_ARGS },
  { "set_hostname",              KW_SET_HOSTNAME },
  { NULL }
};

CfgParser otel_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &otel_debug,
#endif
  .name = "opentelemetry",
  .keywords = otel_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) otel_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(otel_, otel_, void **)
