/*
 * Copyright (c) 2018 Balabit
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
#ifndef LIBTEST_MOCK_CFG_PARSER_H_INCLUDED
#define LIBTEST_MOCK_CFG_PARSER_H_INCLUDED

#include "cfg-lexer.h"

typedef struct
{
  YYSTYPE *yylval;
  YYLTYPE *yylloc;
  CfgLexer *lexer;
} TestParser;

void test_parser_input(TestParser *self, const gchar *buffer);
void test_parser_clear_token(TestParser *self);
void test_parser_next_token(TestParser *self);


TestParser *test_parser_new(void);
void test_parser_free(TestParser *self);

#endif
