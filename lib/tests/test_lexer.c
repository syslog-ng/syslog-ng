/*
 * Copyright (c) 2013 Balabit
 * Copyright (c) 2013 Gergely Nagy <algernon@balabit.hu>
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

#include "testutils.h"
#include "cfg-lexer.h"
#include "cfg-grammar.h"

typedef struct
{
  YYSTYPE *yylval;
  YYLTYPE *yylloc;
  CfgLexer *lexer;
} TestParser;

static void
test_parser_clear_token(TestParser *self)
{
  if (self->yylval->type)
    cfg_lexer_free_token(self->yylval);
  self->yylval->type = 0;
}

static void
test_parser_next_token(TestParser *self)
{
  test_parser_clear_token(self);
  cfg_lexer_lex(self->lexer, self->yylval, self->yylloc);
}

static void
test_parser_input(TestParser *self, const gchar *buffer)
{
  if (self->lexer)
    cfg_lexer_free(self->lexer);
  self->lexer = cfg_lexer_new_buffer(configuration, buffer, strlen(buffer));
}

TestParser *
test_parser_new(void)
{
  TestParser *self = g_new0(TestParser, 1);

  self->yylval = g_new0(YYSTYPE, 1);
  self->yylloc = g_new0(YYLTYPE, 1);
  self->yylval->type = LL_CONTEXT_ROOT;
  self->yylloc->first_column = 1;
  self->yylloc->first_line = 1;
  self->yylloc->last_column = 1;
  self->yylloc->last_line = 1;
  self->yylloc->level = &self->lexer->include_stack[0];
  return self;
}

void
test_parser_free(TestParser *self)
{
  test_parser_clear_token(self);
  if (self->lexer)
    cfg_lexer_free(self->lexer);
  g_free(self->yylval);
  g_free(self->yylloc);
  g_free(self);
}

#define LEXER_TESTCASE(x, ...) do { lexer_testcase_begin(#x, #__VA_ARGS__); x(__VA_ARGS__); lexer_testcase_end(); } while(0)

TestParser *parser = NULL;

#define lexer_testcase_begin(func, args)      \
  do                                                \
    {                                               \
      testcase_begin("%s(%s)", func, args);                     \
      parser = test_parser_new();                               \
    }                                               \
  while (0)

#define lexer_testcase_end()                \
  do                \
    {               \
      test_parser_free(parser);                                 \
      testcase_end();           \
    }               \
  while (0)

static void
_input(const gchar *input)
{
  test_parser_input(parser, input);
}

static void
_next_token(void)
{
  test_parser_next_token(parser);
}

static YYSTYPE *
_current_token(void)
{
  return parser->yylval;
}

static YYLTYPE *
_current_lloc(void)
{
  return parser->yylloc;
}

#define assert_token_type(expected)                                     \
  assert_gint(_current_token()->type, expected, "Bad token type at %s:%d", __FUNCTION__, __LINE__);

#define assert_parser_string(expected)                          \
  _next_token();                                                        \
  assert_token_type(LL_STRING);                                        \
  assert_string(_current_token()->cptr, expected, "Bad parsed string at %s:%d", __FUNCTION__, __LINE__); \

#define assert_parser_block(expected) \
  _next_token();                                                        \
  assert_token_type(LL_BLOCK);                                         \
  assert_string(_current_token()->cptr, expected, "Bad parsed string at %s:%d", __FUNCTION__, __LINE__);

#define assert_parser_block_bad(parser) \
  _next_token();                                                        \
  assert_token_type(LL_ERROR);

#define assert_parser_pragma(parser) \
  _next_token();                                                        \
  assert_token_type(LL_PRAGMA);

#define assert_parser_number(expected) \
  _next_token();                                                        \
  assert_token_type(LL_NUMBER);                                        \
  assert_gint(_current_token()->num, expected, "Bad parsed value at %s:%d", __FUNCTION__, __LINE__);

#define assert_parser_float(expected)                           \
  _next_token();                                                        \
  assert_token_type(LL_FLOAT);                                         \
  assert_true(_current_token()->fnum == expected, "Bad parsed value at %s:%d", __FUNCTION__, __LINE__);

#define assert_parser_identifier(expected) \
  _next_token();                                                        \
  assert_token_type(LL_IDENTIFIER);                                         \
  assert_string(_current_token()->cptr, expected, "Bad parsed value at %s:%d", __FUNCTION__, __LINE__);

#define assert_parser_char(expected) \
  _next_token();                                                        \
  assert_gint(_current_token()->type, expected, "Bad character value at %s:%d", __FUNCTION__, __LINE__);

#define assert_location(line, column) \
  assert_gint(_current_lloc()->first_line, line,          \
              "The line number in the location information "        \
              "does not match the expected value at %s:%d", __FUNCTION__, __LINE__);  \
  assert_gint(_current_lloc()->first_column, column,          \
              "The column number in the location information "        \
              "does not match the expected value at %s:%d", __FUNCTION__, __LINE__);


static gchar *
_format_location_tag_message(void)
{
  EVTCONTEXT *ctx;
  EVTREC *rec;
  char *msg;

  ctx = evt_ctx_init("test", EVT_FAC_USER);
  rec = evt_rec_init(ctx, EVT_PRI_WARNING, "test");
  evt_rec_add_tag(rec, cfg_lexer_format_location_tag(parser->lexer, _current_lloc()));
  msg = evt_format(rec);
  evt_rec_free(rec);
  evt_ctx_free(ctx);

  return msg;
}

#define assert_location_tag(expected) \
  ({                      \
    char *msg = _format_location_tag_message(); \
    const gchar *tag_repr;        \
    tag_repr = strstr(msg, "; ");                 \
    assert_string(tag_repr ? tag_repr + 2 : NULL, expected, "Formatted location tag does not match");  \
    free(msg);                    \
                                                                                        \
  })

static void
test_lexer_string(void)
{
  _input("\"test\"");
  assert_parser_string("test");
  _input("\"test\\x0a\"");
  assert_parser_string("test\n");
  _input("\"test\\o011\"");
  assert_parser_string("test\t");
  _input("\"test\\n\\r\\a\\t\\v\\c\"");
  assert_parser_string("test\n\r\a\t\vc");
}

static void
test_lexer_qstring(void)
{
  _input("'test'");
  assert_parser_string("test");
  _input("'\"test\\n\\r\"'");
  assert_parser_string("\"test\\n\\r\"");
}

#define TEST_BLOCK " {'hello world' \"test value\" {other_block} other\text}"
#define TEST_BAD_BLOCK "this is a bad block starting " TEST_BLOCK

static void
test_lexer_block(void)
{
  _input(TEST_BLOCK);
  cfg_lexer_start_block_state(parser->lexer, "{}");
  assert_parser_block("'hello world' \"test value\" {other_block} other\text");

  _input(TEST_BAD_BLOCK);
  cfg_lexer_start_block_state(parser->lexer, "{}");
  assert_parser_block_bad(parser);
}

static void
test_lexer_others(void)
{
  _input("#This is a full line comment\nfoobar");
  assert_parser_identifier("foobar");
  _input("@version");
  assert_parser_pragma(parser);
  assert_parser_identifier("version");
  _input("():;{}|");
  assert_parser_char('(');
  assert_parser_char(')');
  assert_parser_char(':');
  assert_parser_char(';');
  assert_parser_char('{');
  assert_parser_char('}');
  assert_parser_char('|');
  _input("4.2 12 0x50 011 +12 -12 -4.2 +4.2");
  assert_parser_float(4.2);
  assert_parser_number(12);
  assert_parser_number(80 /*0x50*/);
  assert_parser_number(9 /*011 */);
  assert_parser_number(12);
  assert_parser_number(-12);
  assert_parser_float(-4.2);
  assert_parser_float(4.2);
  _input("test_value");
  assert_parser_identifier("test_value");
}

static void
test_location_tracking(void)
{

  _input("test another\nfoo\nbar\n");
  _next_token();
  assert_location(1, 1);
  _next_token();
  assert_location(1, 6);
  _next_token();
  assert_location(2, 1);
  _next_token();
  assert_location(3, 1);

  assert_location_tag("location='#buffer:3:1'");
}

int
main(int argc, char **argv)
{
  LEXER_TESTCASE(test_lexer_string);
  LEXER_TESTCASE(test_lexer_qstring);
  LEXER_TESTCASE(test_lexer_block);
  LEXER_TESTCASE(test_lexer_others);
  LEXER_TESTCASE(test_location_tracking);
  return 0;
}
