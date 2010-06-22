#include "afprog.h"
#include "cfg-parser.h"
#include "afprog-grammar.h"

extern int afprog_debug;

int afprog_parse(CfgLexer *lexer, LogDriver **instance);

static CfgLexerKeyword afprog_keywords[] = {
  { "program",            KW_PROGRAM },
  { NULL }
};

CfgParser afprog_parser =
{
  .debug_flag = &afprog_debug,
  .name = "afprog",
  .keywords = afprog_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *)) afprog_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(afprog_, LogDriver **)
