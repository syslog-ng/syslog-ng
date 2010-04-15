#include "afsql.h"
#include "cfg-parser.h"
#include "afsql-grammar.h"

extern int afsql_debug;

int afsql_parse(CfgLexer *lexer, LogDriver **instance);

static CfgLexerKeyword afsql_keywords[] = {
#if ENABLE_SQL
  { "sql",                KW_SQL },
  { "username",           KW_USERNAME },
  { "password",           KW_PASSWORD },
  { "database",           KW_DATABASE },
  { "table",              KW_TABLE },

  { "indexes",            KW_INDEXES },
  { "values",             KW_VALUES },
  { "session_statements", KW_SESSION_STATEMENTS, 0x0302 },
#endif
  { NULL }
};

CfgParser afsql_parser =
{
  .debug_flag = &afsql_debug,
  .keywords = afsql_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *)) afsql_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(afsql_, LogDriver **)
