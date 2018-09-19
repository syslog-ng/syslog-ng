add_library(msg-generator STATIC
  msg-generator-parser.c
  msg-generator-parser.h
  msg-generator.c
  msg-generator.h
  msg-generator-source.c
  msg-generator-source.h
  msg-generator-source-options.h
  ${CMAKE_CURRENT_BINARY_DIR}/msg-generator-grammar.c
  ${CMAKE_CURRENT_BINARY_DIR}/msg-generator-grammar.h
)

target_link_libraries(msg-generator PUBLIC syslog-ng)
target_include_directories(msg-generator
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)

generate_y_from_ym(modules/examples/sources/msg-generator/msg-generator-grammar)
bison_target(MsgGeneratorGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/msg-generator-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/msg-generator-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})
