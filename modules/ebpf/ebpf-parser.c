/*
 * Copyright (c) 2023 Balazs Scheidler <bazsi77@gmail.com>
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
#include "driver.h"
#include "ebpf-grammar.h"

extern int ebpf_debug;

int ebpf_parse(CfgLexer *lexer, LogDriverPlugin **instance, gpointer arg);

static CfgLexerKeyword ebpf_keywords[] =
{
  { "ebpf", KW_EBPF },
  { "reuseport", KW_REUSEPORT },
  { "sockets", KW_SOCKETS },
  { NULL }
};

CfgParser ebpf_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &ebpf_debug,
#endif
  .name = "ebpf",
  .keywords = ebpf_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) ebpf_parse,
  .cleanup = (void (*)(gpointer)) log_driver_plugin_free,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(ebpf_, EBPFPROGS_, LogDriverPlugin **)
