#include "filter-expr-parser.h"
#include "filter.h"
#include "filter-expr-grammar.h"

extern int filter_expr_debug;
int filter_expr_parse(CfgLexer *lexer, FilterExprNode **node);

static CfgLexerKeyword filter_expr_keywords[] = {
  { "or",                 KW_OR },
  { "and",                KW_AND },
  { "not",                KW_NOT },
  { "level",              KW_LEVEL },
  { "priority",           KW_LEVEL },
  { "facility",           KW_FACILITY },
  { "program",		  KW_PROGRAM },
  { "host",               KW_HOST },
  { "message",            KW_MESSAGE },
  { "match",		  KW_MATCH },
  { "netmask",		  KW_NETMASK },
  { "type",               KW_TYPE },
  { "tags",               KW_TAGS },
  { NULL }
};

CfgParser filter_expr_parser =
{
  .debug_flag = &filter_expr_debug,
  .keywords = filter_expr_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *)) filter_expr_parse,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(filter_expr_, FilterExprNode **)
