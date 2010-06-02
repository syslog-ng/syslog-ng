#ifndef PRAGMA_PARSER_H_INCLUDED
#define PRAGMA_PARSER_H_INCLUDED

#include "cfg-parser.h"
#include "cfg-lexer.h"
#include "filter.h"

extern CfgParser pragma_parser;

CFG_PARSER_DECLARE_LEXER_BINDING(pragma_, gpointer *)

#endif
