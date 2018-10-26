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
#include "apphook.h"
#include <criterion/criterion.h>
#include "mock-cfg-parser.h"
#include "grab-logging.h"

#define TESTDATA_DIR TOP_SRCDIR "/lib/tests/testdata-lexer"


CfgParserMock *parser = NULL;


static void
_input(const gchar *input)
{
  cfg_parser_mock_input(parser, input);
}

static void
_next_token(void)
{
  cfg_parser_mock_next_token(parser);
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
  cr_assert_str_eq(_current_token()->cptr, expected, "Unexpected string value parsed >>>%s<<< != >>>%s<<<", _current_token()->cptr, expected);

#define assert_parser_token(expected)                          \
  _next_token();                                                        \
  assert_token_type(expected);

#define assert_parser_block(expected) \
  _next_token();                                                        \
  assert_token_type(LL_BLOCK);                                         \
  cr_assert_str_eq(_current_token()->cptr, expected, "Unexpected block value parsed >>>%s<<< != >>>%s<<<", _current_token()->cptr, expected);

#define assert_parser_error() \
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

#define assert_location_range(_first_line, _first_column, _last_line, _last_column) \
  assert_location(_first_line, _first_column); \
  cr_assert_eq(_current_lloc()->last_line, _last_line,          \
              "The last_line number in the location information "        \
              "does not match the expected value, %d != %d", _current_lloc()->last_line, _last_line);  \
  cr_assert_eq(_current_lloc()->last_column, _last_column,          \
              "The last_column number in the location information "        \
              "does not match the expected value, %d != %d", _current_lloc()->last_column, _last_column);


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

Test(lexer, block_token_is_taken_literally_between_a_pair_of_enclosing_characters)
{
  _input(" { hello }");
  cfg_lexer_start_block_state(parser->lexer, "{}");
  assert_parser_block(" hello ");
  assert_location_range(1, 1, 1, 11);

  _input(" { hello\nworld }");
  cfg_lexer_start_block_state(parser->lexer, "{}");
  assert_parser_block(" hello\nworld ");
  assert_location_range(1, 1, 2, 8);

  _input(" { hello\\\nworld }");
  cfg_lexer_start_block_state(parser->lexer, "{}");
  assert_parser_block(" hello\\\nworld ");
  assert_location_range(1, 1, 2, 8);

  _input(" { 'hello' }");
  cfg_lexer_start_block_state(parser->lexer, "{}");
  assert_parser_block(" 'hello' ");
  assert_location_range(1, 1, 1, 13);

  _input(" { 'hello\nworld' }");
  cfg_lexer_start_block_state(parser->lexer, "{}");
  assert_parser_block(" 'hello\nworld' ");
  assert_location_range(1, 1, 2, 9);

  _input(" { 'hello\\\nworld' }");
  cfg_lexer_start_block_state(parser->lexer, "{}");
  assert_parser_block(" 'hello\\\nworld' ");
  assert_location_range(1, 1, 2, 9);

  _input(" { \"hello\" }");
  cfg_lexer_start_block_state(parser->lexer, "{}");
  assert_parser_block(" \"hello\" ");
  assert_location_range(1, 1, 1, 13);

  _input(" { \"hello\nworld\" }");
  cfg_lexer_start_block_state(parser->lexer, "{}");
  assert_parser_block(" \"hello\nworld\" ");
  assert_location_range(1, 1, 2, 9);

  _input(" { \"hello\\\nworld\" }");
  cfg_lexer_start_block_state(parser->lexer, "{}");
  assert_parser_block(" \"hello\\\nworld\" ");
  assert_location_range(1, 1, 2, 9);
}

Test(lexer, block_new_lines_in_text_leading_to_the_opening_bracket_are_tracked_properly)
{
  _input("\n\n\n{ \"hello\\\nworld\" }");
  cfg_lexer_start_block_state(parser->lexer, "{}");
  assert_parser_block(" \"hello\\\nworld\" ");
  assert_location_range(4, 1, 5, 9);

}

Test(lexer, block_closing_brackets_in_embedded_strings_or_comments_dont_close_the_block)
{
  _input(" { 'hello}' }");
  cfg_lexer_start_block_state(parser->lexer, "{}");
  assert_parser_block(" 'hello}' ");

  _input(" { \"hello}\" }");
  cfg_lexer_start_block_state(parser->lexer, "{}");
  assert_parser_block(" \"hello}\" ");

  _input(" {\nfoo# hello}\nbar\n}");
  cfg_lexer_start_block_state(parser->lexer, "{}");
  assert_parser_block("\nfoo# hello}\nbar\n");
}

Test(lexer, block_imbalanced_closing_dont_close_the_block)
{
  _input(" {foo {hello} bar}");
  cfg_lexer_start_block_state(parser->lexer, "{}");
  assert_parser_block("foo {hello} bar");
}

Test(lexer, block_complex_input)
{
  _input("\t \n{'hello world' \"test value\" {other_block} other\text}");
  cfg_lexer_start_block_state(parser->lexer, "{}");
  assert_parser_block("'hello world' \"test value\" {other_block} other\text");
}

Test(lexer, block_input_without_opening_character_is_reported_as_an_error)
{
  start_grabbing_messages();
  _input("this is a block that does not start with an opening brace");
  cfg_lexer_start_block_state(parser->lexer, "{}");
  assert_parser_error();
  assert_grabbed_log_contains("Expected opening bracket as block boundary");
  stop_grabbing_messages();
}

Test(lexer, block_empty_input_in_parens_is_processed_as_a_NULL_pointer)
{
  _input("()");
  cfg_lexer_start_block_state(parser->lexer, "()");
  _next_token();
  assert_token_type(LL_BLOCK);
  cr_assert(_current_token()->cptr == NULL, "%p", _current_token()->cptr);
}

Test(lexer, block_empty_string_in_parens_input_is_processed_as_an_empty_string)
{
  _input("('')");
  cfg_lexer_start_block_state(parser->lexer, "()");
  assert_parser_block("");

  _input("(\"\")");
  cfg_lexer_start_block_state(parser->lexer, "()");
  assert_parser_block("");
}

Test(lexer, at_version_stores_config_version_in_parsed_version_in_hex_form)
{
  parser->lexer->ignore_pragma = FALSE;

  _input("@version: 3.18\n\
foo\n");
  assert_parser_identifier("foo");
  cr_assert_eq(configuration->parsed_version, 0x0312,
               "@version parsing mismatch, value %04x expected %04x", configuration->parsed_version, 0x0312);

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
  _input("..");
  assert_parser_token(LL_DOTDOT);
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

Test(lexer, test_multiline_string_literals)
{
  _input("\"test another\\\nfoo\"\nbar");
  assert_parser_string("test anotherfoo");
  assert_location(1, 1);
  assert_parser_identifier("bar");
  assert_location(3, 1);

  _input("\"test another\nfoo\"\nbar");
  assert_parser_string("test another\nfoo");
  assert_location(1, 1);
  assert_parser_identifier("bar");
  assert_location(3, 1);
}

Test(lexer, test_multiline_qstring_literals)
{
  _input("'test another\nfoo'\nbar");
  assert_parser_string("test another\nfoo");
  assert_location(1, 1);
  assert_parser_identifier("bar");
  assert_location(3, 1);

  _input("'test another\\\nfoo'\nbar");
  assert_parser_string("test another\\\nfoo");
  assert_location(1, 1);
  assert_parser_identifier("bar");
  assert_location(3, 1);
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

Test(lexer, include_file_expands_the_content_of_that_file_in_the_token_stream)
{
  parser->lexer->ignore_pragma = FALSE;
  _input("@include \"" TESTDATA_DIR "/include-test/foo.conf\"\n");
  assert_parser_identifier("foo");
}


Test(lexer, include_wildcard_files_expands_the_content_of_all_files_in_the_token_stream_in_alphabetical_order)
{
  parser->lexer->ignore_pragma = FALSE;
  _input("@include \"" TESTDATA_DIR "/include-test/*.conf\"\n");
  assert_parser_identifier("bar");
  assert_parser_identifier("baz");
  assert_parser_identifier("foo");
}

Test(lexer, include_directory_expands_the_content_of_all_files_in_that_directory_in_alphabetical_ordre)
{
  parser->lexer->ignore_pragma = FALSE;
  _input("@include \"" TESTDATA_DIR "/include-test\"\n");
  assert_parser_identifier("bar");
  assert_parser_identifier("baz");
  assert_parser_identifier("foo");
}

Test(lexer, include_finds_files_in_include_path)
{
  parser->lexer->ignore_pragma = FALSE;
  _input("@define include-path \"" TESTDATA_DIR "/include-test\"\n\
@include foo.conf\n");
  assert_parser_identifier("foo");
}

Test(lexer, include_finds_wildcards_files_in_include_path)
{
  parser->lexer->ignore_pragma = FALSE;
  _input("@define include-path \"" TESTDATA_DIR "/include-test\"\n\
@include \"*.conf\"\n");
  assert_parser_identifier("bar");
  assert_parser_identifier("baz");
  assert_parser_identifier("foo");
}

Test(lexer, include_statement_without_at_sign)
{
  CfgLexer *lexer = parser->lexer;

  cfg_lexer_push_context(parser->lexer, main_parser.context, main_parser.keywords, main_parser.name);
  parser->lexer->ignore_pragma = FALSE;
  _input("@define include-path \"" TESTDATA_DIR "/include-test\"\n\
include \"foo.conf\";\n");
  assert_parser_identifier("foo");
  cfg_lexer_pop_context(lexer);
}

static gboolean
_fake_generator_generate(CfgBlockGenerator *self, GlobalConfig *cfg, CfgArgs *args, GString *result,
                         const gchar *reference)
{
  g_string_append(result, "fake_generator_content");
  return TRUE;
}

CfgBlockGenerator *
fake_generator_new(void)
{
  CfgBlockGenerator *self = g_new0(CfgBlockGenerator, 1);
  cfg_block_generator_init_instance(self, LL_CONTEXT_ROOT, "fake-generator");
  self->generate = _fake_generator_generate;
  return self;
}

Test(lexer, generator_plugins_are_expanded)
{
  CfgLexer *lexer = parser->lexer;

  CfgBlockGenerator *gen = fake_generator_new();
  cfg_lexer_register_generator_plugin(&configuration->plugin_context, gen);
  parser->lexer->ignore_pragma = FALSE;
  cfg_lexer_push_context(parser->lexer, main_parser.context, main_parser.keywords, main_parser.name);
  _input("fake-generator();\n");
  assert_parser_identifier("fake_generator_content");
  cfg_lexer_pop_context(lexer);
}

static void
setup(void)
{
  app_startup();
  configuration = cfg_new_snippet();
  parser = cfg_parser_mock_new();
}

static void
teardown(void)
{
  cfg_parser_mock_free(parser);
  cfg_free(configuration);
  configuration = NULL;
  app_shutdown();
}

TestSuite(lexer, .init = setup, .fini = teardown);
