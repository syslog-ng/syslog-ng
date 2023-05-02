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

#include "cfg-parser.h"
#include "metrics-probe.h"
#include "metrics-probe-grammar.h"

extern int metrics_probe_debug;

int metrics_probe_parse(CfgLexer *lexer, LogParser **instance, gpointer arg);

static CfgLexerKeyword metrics_probe_keywords[] =
{
  { "metrics_probe",               KW_METRICS_PROBE },
  { "key",                         KW_KEY },
  { "labels",                      KW_LABELS },
  { "increment",                   KW_INCREMENT },
  { "level",                       KW_LEVEL },
  { NULL }
};

CfgParser metrics_probe_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &metrics_probe_debug,
#endif
  .name = "metrics-probe",
  .keywords = metrics_probe_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) metrics_probe_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(metrics_probe_, METRICS_PROBE_, LogParser **)
