#include "afsql.h"
#include "cfg-parser.h"
#include "afsql-grammar.h"

extern int afsql_debug;

int afsql_parse(CfgLexer *lexer, LogDriver **instance);

static CfgLexerKeyword afsql_keywords[] = {
  { "sql",                KW_SQL },
  { "username",           KW_USERNAME },
  { "password",           KW_PASSWORD },
  { "database",           KW_DATABASE },
  { "table",              KW_TABLE },

  { "columns",            KW_COLUMNS },
  { "indexes",            KW_INDEXES },
  { "values",             KW_VALUES },
  { "log_fifo_size",      KW_LOG_FIFO_SIZE },
  { "frac_digits",        KW_FRAC_DIGITS },
  { "session_statements", KW_SESSION_STATEMENTS, 0x0302 },
  { "host",               KW_HOST },
  { "port",               KW_PORT },
  { "type",               KW_TYPE },

  { "time_zone",          KW_TIME_ZONE },
  { "local_time_zone",    KW_LOCAL_TIME_ZONE },
  { "null",               KW_NULL },
  { "flush_lines",        KW_FLUSH_LINES },
  { "flush_timeout",      KW_FLUSH_TIMEOUT },
  { "flags",              KW_FLAGS },
  { NULL }
};

CfgParser afsql_parser =
{
  .debug_flag = &afsql_debug,
  .name = "afsql",
  .keywords = afsql_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *)) afsql_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(afsql_, LogDriver **)
