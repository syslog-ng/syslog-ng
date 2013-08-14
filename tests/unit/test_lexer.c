#include "testutils.h"
#include "cfg-lexer.h"
#include "cfg-grammar.h"

typedef struct {
  YYSTYPE *yylval;
  YYLTYPE * yylloc;
  CfgLexer *lexer;
} TestParser;

TestParser *
test_parser_new(const gchar *buffer, gint buffer_len)
{
  TestParser *self = g_new0(TestParser, 1);
  self->lexer = cfg_lexer_new_buffer(buffer, buffer_len);
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
  cfg_lexer_free(self->lexer);
  g_free(self->yylval);
  g_free(self->yylloc);
  g_free(self);
}



#define assert_parser_string(parser, required) \
    assert_gint(_cfg_lexer_lex(parser->yylval, parser->yylloc, parser->lexer->state), LL_STRING, "Bad token type at %s:%d", __FUNCTION__, __LINE__); \
    assert_string(parser->yylval->cptr, required, "Bad parsed string at %s:%d", __FUNCTION__, __LINE__);

#define assert_parser_block(parser, required) \
    assert_gint(_cfg_lexer_lex(parser->yylval, parser->yylloc, parser->lexer->state), LL_BLOCK, "Bad token type at %s:%d", __FUNCTION__, __LINE__); \
    assert_string(parser->yylval->cptr, required, "Bad parsed string at %s:%d", __FUNCTION__, __LINE__);

#define assert_parser_block_bad(parser) \
    assert_gint(_cfg_lexer_lex(parser->yylval, parser->yylloc, parser->lexer->state), LL_ERROR, "Bad token type at %s:%d", __FUNCTION__, __LINE__);

#define assert_parser_pragma(parser) \
    assert_gint(_cfg_lexer_lex(parser->yylval, parser->yylloc, parser->lexer->state), LL_PRAGMA, "Bad token type at %s:%d", __FUNCTION__, __LINE__);

#define assert_parser_number(parser, required) \
    assert_gint(_cfg_lexer_lex(parser->yylval, parser->yylloc, parser->lexer->state), LL_NUMBER, "Bad token type at %s:%d", __FUNCTION__, __LINE__); \
    assert_gint(parser->yylval->num, required, "Bad parsed value at %s:%d", __FUNCTION__, __LINE__);

#define assert_parser_float(parser, required) \
    assert_gint(_cfg_lexer_lex(parser->yylval, parser->yylloc, parser->lexer->state), LL_FLOAT, "Bad token type at %s:%d", __FUNCTION__, __LINE__); \
    assert_true(parser->yylval->fnum == required, "Bad parsed value at %s:%d", __FUNCTION__, __LINE__);

#define assert_parser_identifier(parser, required) \
    assert_gint(_cfg_lexer_lex(parser->yylval, parser->yylloc, parser->lexer->state), LL_IDENTIFIER, "Bad token type at %s:%d", __FUNCTION__, __LINE__); \
    assert_string(parser->yylval->cptr, required, "Bad parsed value at %s:%d", __FUNCTION__, __LINE__);

#define assert_parser_char(parser, required) \
    assert_gint(_cfg_lexer_lex(parser->yylval, parser->yylloc, parser->lexer->state), required, "Bad token type at %s:%d", __FUNCTION__, __LINE__);

#define TEST_STRING "\"test\" \"test\\x0a\" \"test\\o011\" \"test\\n\" \"test\\r\" \"test\\a\" \"test\\t\" \"test\\v\" \"test\\c\""

void
test_lexer_string()
{
  TestParser *parser = test_parser_new(TEST_STRING, strlen(TEST_STRING));
  assert_parser_string(parser, "test");
  assert_parser_string(parser, "test\n");
  assert_parser_string(parser, "test\t");
  assert_parser_string(parser, "test\n");
  assert_parser_string(parser, "test\r");
  assert_parser_string(parser, "test\a");
  assert_parser_string(parser, "test\t");
  assert_parser_string(parser, "test\v");
  assert_parser_string(parser, "testc");
  test_parser_free(parser);
}

#define TEST_QSTRING "'test' '\"test\\n\\r\"'"

void
test_lexer_qstring()
{
  TestParser *parser = test_parser_new(TEST_QSTRING, strlen(TEST_QSTRING));
  assert_parser_string(parser, "test");
  assert_parser_string(parser, "\"test\\n\\r\"");
  test_parser_free(parser);
}

#define TEST_BLOCK " {'hello world' \"test value\" {other_block} other\text}"
#define TEST_BAD_BLOCK "this is a bad block starting " TEST_BLOCK

void
test_lexer_block()
{
  TestParser *parser = test_parser_new(TEST_BLOCK, strlen(TEST_BLOCK));
  _cfg_lexer_force_block_state(parser->lexer->state);
  assert_parser_block(parser, "'hello world' \"test value\" {other_block} other\text");
  test_parser_free(parser);

  parser = test_parser_new(TEST_BAD_BLOCK, strlen(TEST_BAD_BLOCK));
  _cfg_lexer_force_block_state(parser->lexer->state);
  assert_parser_block_bad(parser);
  test_parser_free(parser);
}


#define TEST_VALUES "#This is a full line comment\n@version():;{}| 4.2 12 0x50 011 +12 -12 -4.2 +4.2 test_value"

void
test_lexer_others()
{
  TestParser *parser = test_parser_new(TEST_VALUES, strlen(TEST_VALUES));
  assert_parser_pragma(parser);
  assert_parser_identifier(parser, "version");
  assert_parser_char(parser, '(');
  assert_parser_char(parser, ')');
  assert_parser_char(parser, ':');
  assert_parser_char(parser, ';');
  assert_parser_char(parser, '{');
  assert_parser_char(parser, '}');
  assert_parser_char(parser, '|');
  assert_parser_float(parser, 4.2);
  assert_parser_number(parser, 12);
  assert_parser_number(parser, 80 /*0x50*/);
  assert_parser_number(parser, 9 /*011 */);
  assert_parser_number(parser, 12);
  assert_parser_number(parser, -12);
  assert_parser_float(parser, -4.2);
  assert_parser_float(parser, 4.2);
  assert_parser_identifier(parser, "test_value");
  test_parser_free(parser);
}

int
main(int argc, char **argv)
{
  test_lexer_string();
  test_lexer_qstring();
  test_lexer_block();
  test_lexer_others();
  return 0;
}
