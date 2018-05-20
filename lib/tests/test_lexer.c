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

#include "cfg-lexer.h"
#include "cfg-grammar.h"
#include <criterion/criterion.h>

typedef struct
{
  YYSTYPE *yylval;
  YYLTYPE *yylloc;
  CfgLexer *lexer;
} TestParser;

TestParser *parser = NULL;

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
  cfg_lexer_include_buffer(self->lexer, "#test-buffer", buffer, strlen(buffer));
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

  self->lexer = cfg_lexer_new_buffer(configuration, "", 0);
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
  cr_assert_eq(_current_token()->type, expected, "Unexpected token type %d != %d", _current_token()->type, expected);

#define assert_parser_string(expected)                          \
  _next_token();                                                        \
  assert_token_type(LL_STRING);                                        \
  cr_assert_str_eq(_current_token()->cptr, expected, "Unexpected string value parsed >>>%s<<< != >>>%s<<<", _current_token()->cptr, expected); \

#define assert_parser_block(expected) \
  _next_token();                                                        \
  assert_token_type(LL_BLOCK);                                         \
  cr_assert_str_eq(_current_token()->cptr, expected, "Unexpected block value parsed >>>%s<<< != >>>%s<<<", _current_token()->cptr, expected);

#define assert_parser_block_bad() \
  _next_token();                                                        \
  assert_token_type(LL_ERROR);

#define assert_parser_pragma() \
  _next_token();                                                        \
  assert_token_type(LL_PRAGMA);

#define assert_parser_number(expected) \
  _next_token();                                                        \
  assert_token_type(LL_NUMBER);                                        \
  cr_assert_eq(_current_token()->num, expected, "Unexpected number parsed %" G_GINT64_FORMAT " != %" G_GINT64_FORMAT, (gint64) _current_token()->num, (gint64) expected);

#define assert_parser_float(expected)                           \
  _next_token();                                                        \
  assert_token_type(LL_FLOAT);                                         \
  cr_assert_float_eq(_current_token()->fnum, expected, 1e-7, "Unexpected float parsed %lf != %lf", _current_token()->fnum, expected);

#define assert_parser_identifier(expected) \
  _next_token();                                                        \
  assert_token_type(LL_IDENTIFIER);                                         \
  cr_assert_str_eq(_current_token()->cptr, expected, "Unexpected identifier parsed >>>%s<<< != >>>%s<<<", _current_token()->cptr, expected);

#define assert_parser_char(expected) \
  _next_token();                                                        \
  cr_assert_eq(_current_token()->type, expected, "Unexpected character parsed %c != %c", _current_token()->type, expected);

#define assert_location(line, column) \
  cr_assert_eq(_current_lloc()->first_line, line,          \
              "The line number in the location information "        \
              "does not match the expected value, %d != %d", _current_lloc()->first_line, line);  \
  cr_assert_eq(_current_lloc()->first_column, column,          \
              "The column number in the location information "        \
              "does not match the expected value, %d != %d", _current_lloc()->first_column, column);


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
    cr_assert_str_eq(tag_repr ? tag_repr + 2 : NULL, expected, "Formatted location tag does not match");  \
    free(msg);                    \
                                                                                        \
  })

Test(lexer, test_string)
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

Test(lexer, test_qstring)
{
  _input("'test'");
  assert_parser_string("test");
  _input("'\"test\\n\\r\"'");
  assert_parser_string("\"test\\n\\r\"");
}

#define TEST_BLOCK " {'hello world' \"test value\" {other_block} other\text}"
#define TEST_BAD_BLOCK "this is a bad block starting " TEST_BLOCK

Test(lexer, test_block)
{
  _input(TEST_BLOCK);
  cfg_lexer_start_block_state(parser->lexer, "{}");
  assert_parser_block("'hello world' \"test value\" {other_block} other\text");

  _input(TEST_BAD_BLOCK);
  cfg_lexer_start_block_state(parser->lexer, "{}");
  assert_parser_block_bad();
}

Test(lexer, at_version_stores_config_version_in_parsed_version_in_hex_form)
{
  parser->lexer->ignore_pragma = FALSE;

  _input("@version: 3.15\n\
foo\n");
  assert_parser_identifier("foo");
  cr_assert_eq(configuration->parsed_version, 0x030f,
               "@version parsing mismatch, value %04x expected %04x", configuration->parsed_version, 0x030f);

  _input("@version: 3.1\n\
bar\n");
  assert_parser_identifier("bar");
  cr_assert_eq(configuration->parsed_version, 0x0301,
               "@version parsing mismatch, value %04x expected %04x", configuration->parsed_version, 0x0301);

  _input("@version: 3.5\n\
baz\n");
  assert_parser_identifier("baz");
  cr_assert_eq(configuration->parsed_version, 0x0305,
               "@version parsing mismatch, value %04x expected %04x", configuration->parsed_version, 0x0305);
}

Test(lexer, test_lexer_others)
{
  _input("#This is a full line comment\nfoobar");
  assert_parser_identifier("foobar");
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

Test(lexer, test_location_tracking)
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

Test(lexer, defined_variables_are_substituted_when_enclosed_in_backticks)
{
  parser->lexer->ignore_pragma = FALSE;
  _input("@define var1 value1\n\
@define var2 value2\n\
value0");

  assert_parser_identifier("value0");

  /* we need to supply the variables to be substituted in a separate
   * _input() call as their resolution happens when 1) read from a file, 2)
   * included */

  _input("`var1`\n\
`var2`\n");
  assert_parser_identifier("value1");
  assert_parser_identifier("value2");
}

static void
setup(void)
{
  configuration = cfg_new_snippet();
  parser = test_parser_new();
}

static void
teardown(void)
{
  test_parser_free(parser);
  cfg_free(configuration);
  configuration = NULL;
}

TestSuite(lexer, .init = setup, .fini = teardown);
