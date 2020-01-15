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

module_switch(ENABLE_SNMPTRAPD_PARSER "Enable snmptrapd-parser")

if (ENABLE_SNMPTRAPD_PARSER)
  add_library(snmptrapd-parser SHARED ${SNMPTRAPD_PARSER_SOURCES})
  target_include_directories(snmptrapd-parser
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
  )
  target_link_libraries(snmptrapd-parser PRIVATE syslog-ng)

  install(TARGETS snmptrapd-parser
  LIBRARY DESTINATION lib/syslog-ng/
  COMPONENT snmptrapd-parser)

  add_test_subdirectory(tests)
endif()
