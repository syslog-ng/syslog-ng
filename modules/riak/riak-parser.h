#ifndef RIAK_PARSER_H_INCLUDED
#define RIAK_PARSER_H_INCLUDED

#include "cfg-parser.h"
#include "cfg-lexer.h"
#include "riak.h"

extern CfgParser riak_parser;

CFG_PARSER_DECLARE_LEXER_BINDING(riak_, LogDriver **)

#endif
