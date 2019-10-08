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
#include "mock-cfg-parser.h"
#include "cfg-grammar.h"
#include <string.h>

void
cfg_parser_mock_clear_token(CfgParserMock *self)
{
  if (self->yylval->type)
    cfg_lexer_free_token(self->yylval);
  self->yylval->type = 0;
}

void
cfg_parser_mock_next_token(CfgParserMock *self)
{
  cfg_parser_mock_clear_token(self);
  cfg_lexer_lex(self->lexer, self->yylval, self->yylloc);
}

void
cfg_parser_mock_input(CfgParserMock *self, const gchar *buffer)
{
  cfg_lexer_include_buffer(self->lexer, "#test-buffer", buffer, strlen(buffer));
}

CfgParserMock *
cfg_parser_mock_new(void)
{
  CfgParserMock *self = g_new0(CfgParserMock, 1);

  self->yylval = g_new0(YYSTYPE, 1);
  self->yylloc = g_new0(YYLTYPE, 1);
  self->yylval->type = LL_CONTEXT_ROOT;
  self->yylloc->first_column = 1;
  self->yylloc->first_line = 1;
  self->yylloc->last_column = 1;
  self->yylloc->last_line = 1;
  self->lexer = cfg_lexer_new_buffer(configuration, "", 0);
  self->yylloc->level = &self->lexer->include_stack[0];

  return self;
}

void
cfg_parser_mock_free(CfgParserMock *self)
{
  cfg_parser_mock_clear_token(self);
  if (self->lexer)
    cfg_lexer_free(self->lexer);
  g_free(self->yylval);
  g_free(self->yylloc);
  g_free(self);
}
