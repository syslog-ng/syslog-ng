set(AFPROG_HEADERS
    "afprog.h"
    "afprog-parser.h"
    "${CMAKE_CURRENT_BINARY_DIR}/afprog-grammar.h"
)

set(AFPROG_SOURCES
    "afprog.c"
    "afprog-parser.c"
    "afprog-plugin.c"
    "${CMAKE_CURRENT_BINARY_DIR}/afprog-grammar.c"
)

generate_y_from_ym(modules/afprog/afprog-grammar)

bison_target(AfprogGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/afprog-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/afprog-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

add_library(afprog MODULE ${AFPROG_SOURCES})
target_include_directories(afprog
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(afprog PRIVATE syslog-ng)

install(TARGETS afprog LIBRARY DESTINATION lib/syslog-ng/ COMPONENT afprog)
