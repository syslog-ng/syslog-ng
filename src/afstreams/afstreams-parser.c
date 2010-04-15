#include "afstreams.h"
#include "cfg-parser.h"
#include "afstreams-grammar.h"

extern int afstreams_debug;

int afstreams_parse(CfgLexer *lexer, LogDriver **instance);

static CfgLexerKeyword afstreams_keywords[] = {
#if ENABLE_SUN_STREAMS
  { "sun_stream",         KW_SUN_STREAMS },
  { "sun_streams",        KW_SUN_STREAMS },
#endif
  { "door",               KW_DOOR },
  { NULL }
};

CfgParser afstreams_parser =
{
  .debug_flag = &afstreams_debug,
  .keywords = afstreams_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *)) afstreams_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(afstreams_, LogDriver **)
