#ifndef AFPROG_PARSER_H_INCLUDED
#define AFPROG_PARSER_H_INCLUDED

#include "cfg-parser.h"
#include "cfg-lexer.h"
#include "driver.h"

extern CfgParser afprog_parser;

CFG_PARSER_DECLARE_LEXER_BINDING(afprog_, LogDriver **)

#endif
