

find_package(rdkafka 1.0.0)

if (RDKAFKA_FOUND)
  option(ENABLE_KAFKA "Enable kafka module" ON)
else()
  option(ENABLE_KAFKA "Enable kafka module" OFF)
endif()

if (NOT ENABLE_KAFKA)
 return()
endif()

if (NOT RDKAFKA_FOUND)
  message(FATAL_ERROR "Kafka enabled but no librdkafka found")
endif()


add_library(kafka SHARED
  kafka-parser.c
  kafka-parser.h
  kafka-plugin.c
  kafka-dest-driver.c
  kafka-dest-worker.c
  kafka-plugin.c
  kafka-props.c
  ${CMAKE_CURRENT_BINARY_DIR}/kafka-grammar.c
  ${CMAKE_CURRENT_BINARY_DIR}/kafka-grammar.h
)

target_link_libraries(kafka PUBLIC syslog-ng rdkafka)
target_include_directories(kafka
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)

generate_y_from_ym(modules/kafka/kafka-grammar)
bison_target(KafkaGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/kafka-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/kafka-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

add_test_subdirectory(tests)


install(TARGETS kafka LIBRARY DESTINATION lib/syslog-ng/ COMPONENT kafka)
