#include "afuser.h"
#include "cfg-parser.h"
#include "afuser-grammar.h"

extern int afuser_debug;

int afuser_parse(CfgLexer *lexer, LogDriver **instance);

static CfgLexerKeyword afuser_keywords[] = {
  { "usertty",               KW_USERTTY },

  { NULL }
};

CfgParser afuser_parser =
{
  .debug_flag = &afuser_debug,
  .name = "afuser",
  .keywords = afuser_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *)) afuser_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(afuser_, LogDriver **)
