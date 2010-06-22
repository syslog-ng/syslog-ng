#ifndef REWRITE_EXPR_PARSER_H_INCLUDED
#define REWRITE_EXPR_PARSER_H_INCLUDED

#include "cfg-parser.h"
#include "cfg-lexer.h"

extern CfgParser rewrite_expr_parser;

CFG_PARSER_DECLARE_LEXER_BINDING(rewrite_expr_, GList **)

#endif
