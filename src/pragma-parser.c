#include "pragma-parser.h"
#include "pragma-grammar.h"

extern int pragma_debug;
int pragma_parse(CfgLexer *lexer, gpointer *result);

static CfgLexerKeyword pragma_keywords[] = {
  { "version",            KW_VERSION, },
  { "include",            KW_INCLUDE, 0x0302, },
  { "module",             KW_MODULE, 0x0302, },
  { "define",             KW_DEFINE, 0x0302, },
  { CFG_KEYWORD_STOP },
};

CfgParser pragma_parser =
{
  .debug_flag = &pragma_debug,
  .name = "pragma",
  .context = LL_CONTEXT_PRAGMA,
  .keywords = pragma_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *)) pragma_parse,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(pragma_, gpointer *)
