module_switch(ENABLE_SNMPTRAPD_PARSER "Enable snmptrapd-parser")
if (NOT ENABLE_SNMPTRAPD_PARSER)
  return()
endif()

set(SNMPTRAPD_PARSER_SOURCES
    "snmptrapd-parser-parser.h"
    "snmptrapd-parser-plugin.c"
    "snmptrapd-parser-parser.c"
    "snmptrapd-header-parser.c"
    "snmptrapd-parser.c"
    "varbindlist-scanner.c"
)

add_module(
  TARGET snmptrapd-parser
  GRAMMAR snmptrapd-parser-grammar
  SOURCES ${SNMPTRAPD_PARSER_SOURCES}
)

add_test_subdirectory(tests)

