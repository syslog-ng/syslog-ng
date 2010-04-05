#include "dummy.h"
#include "cfg-parser.h"
#include "dummy-grammar.h"

extern int dummy_debug;
int dummy_parse(CfgLexer *lexer, DummyDestDriver **instance);

static CfgLexerKeyword dummy_keywords[] = {
  { "dummy",                    KW_DUMMY },
  { "dummy_opt", 		KW_DUMMY_OPT },
  { NULL }
};

CfgParser dummy_parser =
{
  .debug_flag = &dummy_debug,
  .keywords = dummy_keywords,
  .parse = (int (*)(CfgLexer *lexer, gpointer *instance)) dummy_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(dummy_, DummyDestDriver **)
