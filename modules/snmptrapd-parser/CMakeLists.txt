set(snmptrapd_parser_HEADERS
    "snmptrapd-parser-parser.h"
    "${CMAKE_CURRENT_BINARY_DIR}/snmptrapd-parser-grammar.h"
)

set(SNMPTRAPD_PARSER_SOURCES
    "snmptrapd-parser-plugin.c"
    "snmptrapd-parser-parser.c"
    "snmptrapd-header-parser.c"
    "snmptrapd-parser.c"
    "varbindlist-scanner.c"
    "${CMAKE_CURRENT_BINARY_DIR}/snmptrapd-parser-grammar.c"
)

generate_y_from_ym(modules/snmptrapd-parser/snmptrapd-parser-grammar)

bison_target(snmptrapd-parserGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/snmptrapd-parser-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/snmptrapd-parser-grammar.c
COMPILE_FLAGS ${BISON_FLAGS})

option(ENABLE_SNMPTRAPD_PARSER "Enable snmptrapd-parser" ON)

if (ENABLE_SNMPTRAPD_PARSER)
  include_directories (${CMAKE_CURRENT_BINARY_DIR})
  include_directories (${CMAKE_CURRENT_SOURCE_DIR})
  add_library(snmptrapd_parser SHARED ${SNMPTRAPD_PARSER_SOURCES})
  target_link_libraries(snmptrapd_parser PRIVATE syslog-ng)

  install(TARGETS snmptrapd_parser
  LIBRARY DESTINATION lib/syslog-ng/
  COMPONENT snmptrapd_parser)

  add_subdirectory(tests)
endif()
