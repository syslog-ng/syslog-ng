/*
 * Copyright (c) 2014 Balabit
 * Copyright (c) 2014 Viktor Juhasz <viktor.juhasz@balabit.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "cfg-parser.h"
#include "java-grammar.h"

int java_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword java_keywords[] =
{
  { "java",        KW_JAVA },
  { "class_path",  KW_CLASS_PATH},
  { "class_name",  KW_CLASS_NAME},
  { "option",      KW_OPTION},

  { "jvm_options", KW_JVM_OPTIONS},
  { NULL }
};

CfgParser java_parser =
{
  .name = "java",
  .keywords = java_keywords,
  .parse = (int (*)(CfgLexer *lexer, gpointer *instance, gpointer)) java_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(java_, LogDriver **)
