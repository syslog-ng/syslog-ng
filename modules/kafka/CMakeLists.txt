find_package(rdkafka 1.1.0)

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


set(CMAKE_REQUIRED_INCLUDES ${RDKAFKA_INCLUDE_DIR})
set(CMAKE_REQUIRED_LIBRARIES ${RDKAFKA_LIBRARY})
check_symbol_exists(rd_kafka_init_transactions "librdkafka/rdkafka.h" SYSLOG_NG_HAVE_RD_KAFKA_INIT_TRANSACTIONS)

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

