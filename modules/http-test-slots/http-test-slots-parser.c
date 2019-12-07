#include "http-test-slots.h"
#include "cfg-parser.h"
#include "http-test-slots-grammar.h"

extern int http_test_slots_debug;
int http_test_slots_parse(CfgLexer *lexer, LogDriverPlugin **instance, gpointer arg);

static CfgLexerKeyword http_test_slots_keywords[] =
{
  { "http_test_slots",  KW_HTTP_TEST_SLOTS },
  { "header", KW_HEADER },
  { NULL }
};

CfgParser http_test_slots_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &http_test_slots_debug,
#endif
  .name = "http_test_slots",
  .keywords = http_test_slots_keywords,
  .parse = (int (*)(CfgLexer *lexer, gpointer *instance, gpointer arg)) http_test_slots_parse,
  .cleanup = (void (*)(gpointer)) log_driver_plugin_free
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(http_test_slots_, LogDriverPlugin **)
