#include "afsocket.h"
#include "cfg-parser.h"
#include "afsocket-grammar.h"

extern int afsocket_debug;

int afsocket_parse(CfgLexer *lexer, LogDriver **instance);

static CfgLexerKeyword afsocket_keywords[] = {
  { "unix_dgram",	KW_UNIX_DGRAM },
  { "unix_stream",	KW_UNIX_STREAM },
  { "udp",                KW_UDP },
  { "tcp",                KW_TCP },
#if ENABLE_IPV6
  { "udp6",               KW_UDP6 },
  { "tcp6",               KW_TCP6 },
#endif
  { "syslog",             KW_SYSLOG },
  { NULL }
};

CfgParser afsocket_parser =
{
  .debug_flag = &afsocket_debug,
  .keywords = afsocket_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *)) afsocket_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(afsocket_, LogDriver **)
