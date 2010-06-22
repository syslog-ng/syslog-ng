#ifndef AFUSER_PARSER_H_INCLUDED
#define AFUSER_PARSER_H_INCLUDED

#include "cfg-parser.h"
#include "cfg-lexer.h"
#include "driver.h"

extern CfgParser afuser_parser;

CFG_PARSER_DECLARE_LEXER_BINDING(afuser_, LogDriver **)

#endif
