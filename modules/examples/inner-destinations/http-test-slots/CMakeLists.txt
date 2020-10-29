set(HTTP_TEST_SLOTS_HEADERS
    "${CMAKE_CURRENT_BINARY_DIR}/http-test-slots-grammar.h"
    "http-test-slots-parser.h"
    "http-test-slots.h"
)

set(HTTP_TEST_SLOTS_SOURCES
    "${CMAKE_CURRENT_BINARY_DIR}/http-test-slots-grammar.c"
    "http-test-slots-parser.c"
    "http-test-slots.c"
)

generate_y_from_ym(modules/examples/inner-destinations/http-test-slots/http-test-slots-grammar)

BISON_GEN(
  NAME HttpTestSlotsGrammar
  INPUT ${CMAKE_CURRENT_BINARY_DIR}/http-test-slots-grammar.y
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/http-test-slots-grammar
  COMPILE_FLAGS ${BISON_FLAGS})

add_library(http-test-slots STATIC ${HTTP_TEST_SLOTS_SOURCES})

target_include_directories(http-test-slots
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
  PUBLIC ${PROJECT_SOURCE_DIR}
)

target_link_libraries(http-test-slots PUBLIC syslog-ng)

