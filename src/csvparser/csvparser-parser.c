#include "csvparser.h"
#include "cfg-parser.h"
#include "csvparser-grammar.h"

extern int csvparser_debug;

int csvparser_parse(CfgLexer *lexer, LogParser **instance);

static CfgLexerKeyword csvparser_keywords[] =
{
  { "csv_parser",          KW_CSV_PARSER, 0x0300 },
  { NULL }
};

CfgParser csvparser_parser =
{
  .debug_flag = &csvparser_debug,
  .name = "csvparser",
  .keywords = csvparser_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *)) csvparser_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(csvparser_, LogParser **)
