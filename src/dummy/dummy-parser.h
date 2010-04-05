#ifndef DUMMY_PARSER_H_INCLUDED
#define DUMMY_PARSER_H_INCLUDED

#include "cfg-parser.h"
#include "cfg-lexer.h"
#include "dummy.h"

extern CfgParser dummy_parser;

CFG_PARSER_DECLARE_LEXER_BINDING(dummy_, DummyDestDriver **)

#endif
