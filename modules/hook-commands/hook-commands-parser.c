/*
 * Copyright (c) 2002-2016 Balabit
 * Copyright (c) 2016 Viktor Juhasz <viktor.juhasz@balabit.com>
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

#include "hook-commands.h"
#include "cfg-parser.h"
#include "hook-commands-grammar.h"

extern int hook_commands_debug;
int hook_commands_parse(CfgLexer *lexer, LogDriverPlugin **instance, gpointer arg);

static CfgLexerKeyword hook_commands_keywords[] =
{
  { "hook_commands",   KW_HOOK_COMMANDS },
  { "startup",         KW_STARTUP },
  { "setup",           KW_SETUP },
  { "teardown",        KW_TEARDOWN },
  { "shutdown",        KW_SHUTDOWN },
  { NULL }
};

CfgParser hook_commands_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &hook_commands_debug,
#endif
  .name = "hook_commands",
  .keywords = hook_commands_keywords,
  .parse = (int (*)(CfgLexer *lexer, gpointer *instance, gpointer arg)) hook_commands_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,

};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(hook_commands_, LogDriverPlugin **)
