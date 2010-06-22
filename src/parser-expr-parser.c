#include "parser-expr-parser.h"
#include "logparser.h"
#include "parser-expr-grammar.h"

extern int parser_expr_debug;
int parser_expr_parse(CfgLexer *lexer, GList **node);

static CfgLexerKeyword parser_expr_keywords[] = {
  { "file",               KW_FILE },
  { NULL }
};

CfgParser parser_expr_parser =
{
  .debug_flag = &parser_expr_debug,
  .name = "parser expression",
  .context = LL_CONTEXT_PARSER,
  .keywords = parser_expr_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *)) parser_expr_parse,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(parser_expr_, GList **)
