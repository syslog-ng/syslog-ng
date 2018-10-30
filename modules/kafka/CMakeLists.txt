add_library(kafka SHARED
  kafka-parser.c
  kafka-parser.h
  kafka-source.c
  kafka-source.h
  kafka-plugin.c
  ${CMAKE_CURRENT_BINARY_DIR}/kafka-grammar.c
  ${CMAKE_CURRENT_BINARY_DIR}/kafka-grammar.h
)

target_link_libraries(kafka PUBLIC syslog-ng)
target_include_directories(kafka
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)

generate_y_from_ym(modules/kafka/kafka-grammar)
bison_target(KafkaGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/kafka-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/kafka-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})
