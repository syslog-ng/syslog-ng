#include "rewrite-expr-parser.h"
#include "logrewrite.h"
#include "rewrite-expr-grammar.h"

extern int rewrite_expr_debug;
int rewrite_expr_parse(CfgLexer *lexer, GList **node);

static CfgLexerKeyword rewrite_expr_keywords[] = {
  { "set",                KW_SET, 0x0300 },
  { "subst",              KW_SUBST, 0x0300 },

  { "type",               KW_TYPE, 0x0300 },
  { "flags",              KW_FLAGS },
  { NULL }
};

CfgParser rewrite_expr_parser =
{
  .debug_flag = &rewrite_expr_debug,
  .name = "rewrite expression",
  .context = LL_CONTEXT_REWRITE,
  .keywords = rewrite_expr_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *)) rewrite_expr_parse,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(rewrite_expr_, GList **)
