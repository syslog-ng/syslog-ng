/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2023-2024 Attila Szakacs <attila.szakacs@axoflow.com>
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
#include "grpc-parser.h"

extern int otel_debug;

int otel_parse(CfgLexer *lexer, void **instance, gpointer arg);

static CfgLexerKeyword otel_keywords[] =
{
  GRPC_KEYWORDS,
  { "opentelemetry",             KW_OPENTELEMETRY },
  { "syslog_ng_otlp",            KW_SYSLOG_NG_OTLP },
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
