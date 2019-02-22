/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2018 László Várady <laszlo.varady@balabit.com>
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
#include "msg-generator-grammar.h"

extern int msg_generator_debug;

int msg_generator_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword msg_generator_keywords[] =
{
  { "example_msg_generator", KW_MSG_GENERATOR },
  { "freq", KW_FREQ },
  { "num", KW_NUM },
  { "template", KW_TEMPLATE },
  { NULL }
};

CfgParser msg_generator_parser =
{
#if ENABLE_DEBUG
  .debug_flag = &msg_generator_debug,
#endif
  .name = "msg_generator",
  .keywords = msg_generator_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) msg_generator_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(msg_generator_, LogDriver **)

