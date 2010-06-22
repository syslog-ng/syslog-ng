#include "dbparser.h"
#include "cfg-parser.h"
#include "dbparser-grammar.h"

extern int dbparser_debug;

int dbparser_parse(CfgLexer *lexer, LogParser **instance);

static CfgLexerKeyword dbparser_keywords[] =
{
  { "db_parser",          KW_DB_PARSER, 0x0300 },
  { NULL }
};

CfgParser dbparser_parser =
{
  .debug_flag = &dbparser_debug,
  .name = "dbparser",
  .keywords = dbparser_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *)) dbparser_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(dbparser_, LogParser **)
