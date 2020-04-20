add_library(threaded-diskq-source STATIC
  threaded-diskq-source-parser.c
  threaded-diskq-source-parser.h
  threaded-diskq-source.c
  threaded-diskq-source.h
  ${CMAKE_CURRENT_BINARY_DIR}/threaded-diskq-source-grammar.c
  ${CMAKE_CURRENT_BINARY_DIR}/threaded-diskq-source-grammar.h
)

target_link_libraries(threaded-diskq-source PUBLIC syslog-ng PRIVATE syslog-ng-disk-buffer)
target_include_directories(threaded-diskq-source
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)

generate_y_from_ym(modules/examples/sources/threaded-diskq-source/threaded-diskq-source-grammar)
bison_target(MsgGeneratorGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/threaded-diskq-source-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/threaded-diskq-source-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})
