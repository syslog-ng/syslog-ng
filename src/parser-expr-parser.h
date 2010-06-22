#ifndef PARSER_EXPR_PARSER_H_INCLUDED
#define PARSER_EXPR_PARSER_H_INCLUDED

#include "cfg-parser.h"
#include "cfg-lexer.h"
#include "logparser.h"

extern CfgParser parser_expr_parser;

CFG_PARSER_DECLARE_LEXER_BINDING(parser_expr_, GList **)

#endif
