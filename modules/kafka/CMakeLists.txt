if (NOT DEFINED ENABLE_KAFKA OR ENABLE_KAFKA)
  find_package(rdkafka 1.1.0)
endif ()

module_switch (ENABLE_KAFKA "Enable kafka module" RDKAFKA_FOUND)

if (ENABLE_KAFKA AND NOT RDKAFKA_FOUND)
  message (FATAL_ERROR "Kafka enabled but no librdkafka found")
endif ()

if (NOT ENABLE_KAFKA)
  return()
endif()

check_symbol_exists (rd_kafka_init_transactions "librdkafka/rdkafka.h" SYSLOG_NG_HAVE_RD_KAFKA_INIT_TRANSACTIONS)

set(CMAKE_REQUIRED_INCLUDES ${RDKAFKA_INCLUDE_DIR})
set(CMAKE_REQUIRED_LIBRARIES ${RDKAFKA_LIBRARY})

set(KAFKA_SOURCES
  kafka-parser.c
  kafka-parser.h
  kafka-plugin.c
  kafka-dest-driver.c
  kafka-dest-worker.c
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

