#include "riak.h"
#include "cfg-parser.h"
#include "riak-grammar.h"

extern int riak_debug;
int riak_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword riak_keywords[] = {
  { "riak",			KW_RIAK },
  { "host",		    KW_HOST },
  { "port",			KW_PORT },
  { "bucket",		KW_BUCKET },
  { "key",			KW_KEY },
  { "value",		KW_VALUE },
  { "mode",			KW_MODE },
  { "type",		    KW_TYPE },
  { "charset",      KW_CHARSET },
  { "content_type",        KW_CONTENT_TYPE },
  { "flush_lines",      KW_FLUSH_LINES },
  { NULL }
};

CfgParser riak_parser =
{
#if ENABLE_DEBUG
  .debug_flag = &riak_debug,
#endif
  .name = "riak",
  .keywords = riak_keywords,
  .parse = (int (*)(CfgLexer *lexer, gpointer *instance, gpointer)) riak_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(riak_, LogDriver **)

