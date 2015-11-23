#include "testutils.h"
#include "cfg-lexer.h"
#include "cfg-grammar.h"

typedef struct {
  YYSTYPE *yylval;
  YYLTYPE * yylloc;
  CfgLexer *lexer;
} TestParser;

static void
test_parser_clear_token(TestParser *self)
{
  cfg_lexer_free_token(self->yylval);
}

static void
test_parser_input(TestParser *self, const gchar *buffer)
{
  if (self->lexer)
    cfg_lexer_free(self->lexer);
  self->lexer = cfg_lexer_new_buffer(buffer, strlen(buffer));
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
  if (self->lexer)
    cfg_lexer_free(self->lexer);
  g_free(self->yylval);
  g_free(self->yylloc);
  g_free(self);
}

#define LEXER_TESTCASE(x, ...) do { lexer_testcase_begin(#x, #__VA_ARGS__); x(__VA_ARGS__); lexer_testcase_end(); } while(0)

TestParser *parser = NULL;

#define lexer_testcase_begin(func, args) 			\
  do                                          			\
    {                                         			\
      testcase_begin("%s(%s)", func, args);                     \
      parser = test_parser_new();                               \
    }                                         			\
  while (0)

#define lexer_testcase_end()				        \
  do								\
    {								\
      test_parser_free(parser);                                 \
      testcase_end();						\
    }								\
  while (0)


#define assert_parser_string(parser, required) \
  assert_gint(cfg_lexer_lex(parser->lexer, parser->yylval, parser->yylloc), LL_STRING, "Bad token type at %s:%d", __FUNCTION__, __LINE__); \
  assert_string(parser->yylval->cptr, required, "Bad parsed string at %s:%d", __FUNCTION__, __LINE__); \
  test_parser_clear_token(parser);

#define assert_parser_block(parser, required) \
  assert_gint(cfg_lexer_lex(parser->lexer, parser->yylval, parser->yylloc), LL_BLOCK, "Bad token type at %s:%d", __FUNCTION__, __LINE__); \
  assert_string(parser->yylval->cptr, required, "Bad parsed string at %s:%d", __FUNCTION__, __LINE__); \
  test_parser_clear_token(parser);

#define assert_parser_block_bad(parser) \
  assert_gint(cfg_lexer_lex(parser->lexer, parser->yylval, parser->yylloc), LL_ERROR, "Bad token type at %s:%d", __FUNCTION__, __LINE__); \
  test_parser_clear_token(parser);

#define assert_parser_pragma(parser) \
  assert_gint(cfg_lexer_lex(parser->lexer, parser->yylval, parser->yylloc), LL_PRAGMA, "Bad token type at %s:%d", __FUNCTION__, __LINE__); \
  test_parser_clear_token(parser);

#define assert_parser_number(parser, required) \
  assert_gint(cfg_lexer_lex(parser->lexer, parser->yylval, parser->yylloc), LL_NUMBER, "Bad token type at %s:%d", __FUNCTION__, __LINE__); \
  assert_gint(parser->yylval->num, required, "Bad parsed value at %s:%d", __FUNCTION__, __LINE__); \
  test_parser_clear_token(parser);

#define assert_parser_float(parser, required)                           \
  assert_gint(cfg_lexer_lex(parser->lexer, parser->yylval, parser->yylloc), LL_FLOAT, "Bad token type at %s:%d", __FUNCTION__, __LINE__); \
  assert_true(parser->yylval->fnum == required, "Bad parsed value at %s:%d", __FUNCTION__, __LINE__); \
  test_parser_clear_token(parser);

#define assert_parser_identifier(parser, required) \
  assert_gint(cfg_lexer_lex(parser->lexer, parser->yylval, parser->yylloc), LL_IDENTIFIER, "Bad token type at %s:%d", __FUNCTION__, __LINE__); \
  assert_string(parser->yylval->cptr, required, "Bad parsed value at %s:%d", __FUNCTION__, __LINE__); \
  test_parser_clear_token(parser);

#define assert_parser_char(parser, required) \
  assert_gint(cfg_lexer_lex(parser->lexer, parser->yylval, parser->yylloc), required, "Bad token type at %s:%d", __FUNCTION__, __LINE__); \
  test_parser_clear_token(parser);

#define TEST_STRING "\"test\" \"test\\x0a\" \"test\\o011\" \"test\\n\" \"test\\r\" \"test\\a\" \"test\\t\" \"test\\v\" \"test\\c\""

void
test_lexer_string()
{
  test_parser_input(parser, TEST_STRING);
  assert_parser_string(parser, "test");
  assert_parser_string(parser, "test\n");
  assert_parser_string(parser, "test\t");
  assert_parser_string(parser, "test\n");
  assert_parser_string(parser, "test\r");
  assert_parser_string(parser, "test\a");
  assert_parser_string(parser, "test\t");
  assert_parser_string(parser, "test\v");
  assert_parser_string(parser, "testc");
}

#define TEST_QSTRING "'test' '\"test\\n\\r\"'"

void
test_lexer_qstring()
{
  test_parser_input(parser, TEST_QSTRING);
  assert_parser_string(parser, "test");
  assert_parser_string(parser, "\"test\\n\\r\"");
}

#define TEST_BLOCK " {'hello world' \"test value\" {other_block} other\text}"
#define TEST_BAD_BLOCK "this is a bad block starting " TEST_BLOCK

void
test_lexer_block()
{
  test_parser_input(parser, TEST_BLOCK);
  cfg_lexer_start_block_state(parser->lexer, "{}");
  assert_parser_block(parser, "'hello world' \"test value\" {other_block} other\text");

  test_parser_input(parser, TEST_BAD_BLOCK);
  cfg_lexer_start_block_state(parser->lexer, "{}");
  assert_parser_block_bad(parser);
}


#define TEST_VALUES "#This is a full line comment\n@version():;{}| 4.2 12 0x50 011 +12 -12 -4.2 +4.2 test_value"

void
test_lexer_others()
{
  test_parser_input(parser, TEST_VALUES);
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
}

int
main(int argc, char **argv)
{
  LEXER_TESTCASE(test_lexer_string);
  LEXER_TESTCASE(test_lexer_qstring);
  LEXER_TESTCASE(test_lexer_block);
  LEXER_TESTCASE(test_lexer_others);
  return 0;
}
