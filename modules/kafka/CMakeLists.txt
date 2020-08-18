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

set(KAFKA_SOURCES
  kafka-parser.c
  kafka-parser.h
  kafka-plugin.c
  kafka-dest-driver.c
  kafka-dest-worker.c
  kafka-plugin.c
  kafka-props.c
  kafka-internal.h
)

add_module(
  TARGET kafka
  GRAMMAR kafka-grammar
  DEPENDS rdkafka
  SOURCES ${KAFKA_SOURCES}
)

add_test_subdirectory(tests)

