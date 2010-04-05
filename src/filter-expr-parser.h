#ifndef FILTER_EXPR_PARSER_H_INCLUDED
#define FILTER_EXPR_PARSER_H_INCLUDED

#include "cfg-parser.h"
#include "cfg-lexer.h"
#include "filter.h"

extern CfgParser filter_expr_parser;

CFG_PARSER_DECLARE_LEXER_BINDING(filter_expr_, FilterExprNode **)

#endif
