if (NOT SYSLOG_NG_HAVE_GETRANDOM)
  add_library(threaded-random-generator INTERFACE)
  return()
endif()

add_library(threaded-random-generator STATIC
  threaded-random-generator-parser.c
  threaded-random-generator-parser.h
  threaded-random-generator.c
  threaded-random-generator.h
  ${CMAKE_CURRENT_BINARY_DIR}/threaded-random-generator-grammar.c
  ${CMAKE_CURRENT_BINARY_DIR}/threaded-random-generator-grammar.h
)

target_link_libraries(threaded-random-generator PUBLIC syslog-ng)
target_include_directories(threaded-random-generator
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)

generate_y_from_ym(modules/examples/sources/threaded-random-generator/threaded-random-generator-grammar)
bison_target(MsgGeneratorGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/threaded-random-generator-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/threaded-random-generator-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})
