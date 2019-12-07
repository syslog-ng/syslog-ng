#ifndef HTTP_TEST_SLOTS_PARSER_H_INCLUDED
#define HTTP_TEST_SLOTS_PARSER_H_INCLUDED

#include "cfg-parser.h"
#include "driver.h"

extern CfgParser http_test_slots_parser;

CFG_PARSER_DECLARE_LEXER_BINDING(http_test_slots_, LogDriverPlugin **);

#endif
