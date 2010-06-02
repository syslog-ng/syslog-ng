#ifndef BLOCK_REF_PARSER_H_INCLUDED
#define BLOCK_REF_PARSER_H_INCLUDED

#include "cfg-parser.h"
#include "cfg-lexer.h"
#include "filter.h"

extern CfgParser block_ref_parser;
extern CfgLexerKeyword *block_def_keywords;

CFG_PARSER_DECLARE_LEXER_BINDING(block_ref_, CfgArgs **)

#endif
