/*
 * Copyright (c) 2020 One Identity
 * Copyright (c) 2020 Laszlo Budai <laszlo.budai@outlook.com>
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

#include "http-test-slots.h"
#include "cfg-parser.h"
#include "http-test-slots-grammar.h"

extern int http_test_slots_debug;
int http_test_slots_parse(CfgLexer *lexer, LogDriverPlugin **instance, gpointer arg);

static CfgLexerKeyword http_test_slots_keywords[] =
{
  { "http_test_slots",  KW_HTTP_TEST_SLOTS },
  { "header", KW_HEADER },
  { NULL }
};

CfgParser http_test_slots_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &http_test_slots_debug,
#endif
  .name = "http_test_slots",
  .keywords = http_test_slots_keywords,
  .parse = (int (*)(CfgLexer *lexer, gpointer *instance, gpointer arg)) http_test_slots_parse,
  .cleanup = (void (*)(gpointer)) log_driver_plugin_free
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(http_test_slots_, LogDriverPlugin **)
