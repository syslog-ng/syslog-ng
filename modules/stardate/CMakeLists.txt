set(stardate_HEADERS
    "stardate-parser.h"
    "${CMAKE_CURRENT_BINARY_DIR}/stardate-grammar.h"
)

set(stardate_SOURCES
    "stardate-plugin.c"
    "stardate-parser.c"
    "${CMAKE_CURRENT_BINARY_DIR}/stardate-grammar.c"
)

generate_y_from_ym(modules/stardate/stardate-grammar)

bison_target(stardateGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/stardate-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/stardate-grammar.c
COMPILE_FLAGS ${BISON_FLAGS})

option(ENABLE_stardate "Enable stardate ON")

if (ENABLE_stardate)
  include_directories (${CMAKE_CURRENT_BINARY_DIR})
  include_directories (${CMAKE_CURRENT_SOURCE_DIR})
  add_library(stardate MODULE ${stardate_SOURCES})
  target_link_libraries(stardate PRIVATE syslog-ng)

  install(TARGETS stardate
  LIBRARY DESTINATION lib/syslog-ng/
  COMPONENT stardate)
endif()
