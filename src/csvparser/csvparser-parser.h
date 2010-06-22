#ifndef DBPARSER_PARSER_H_INCLUDED
#define DBPARSER_PARSER_H_INCLUDED

#include "cfg-parser.h"
#include "cfg-lexer.h"
#include "logparser.h"

extern CfgParser dbparser_parser;

CFG_PARSER_DECLARE_LEXER_BINDING(dbparser_, LogParser **)

#endif
