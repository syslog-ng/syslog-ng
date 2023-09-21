set(TLS_TEST_VALIDATION_HEADERS
    "${CMAKE_CURRENT_BINARY_DIR}/tls-test-validation-grammar.h"
    "tls-test-validation-parser.h"
    "tls-test-validation.h"
)

set(TLS_TEST_VALIDATION_SOURCES
    "${CMAKE_CURRENT_BINARY_DIR}/tls-test-validation-grammar.c"
    "tls-test-validation-parser.c"
    "tls-test-validation.c"
)

generate_y_from_ym(modules/examples/inner-destinations/tls-test-validation/tls-test-validation-grammar)

bison_target(HttpTestSlotsGrammar
      ${CMAKE_CURRENT_BINARY_DIR}/tls-test-validation-grammar.y
      ${CMAKE_CURRENT_BINARY_DIR}/tls-test-validation-grammar.c
      COMPILE_FLAGS ${BISON_FLAGS})
set_source_files_properties(
      ${CMAKE_CURRENT_BINARY_DIR}/tls-test-validation-grammar.c
      PROPERTIES
      COMPILE_FLAGS ${BISON_BUILT_SOURCE_CFLAGS}
)

add_library(tls-test-validation STATIC ${TLS_TEST_VALIDATION_SOURCES})

target_include_directories(tls-test-validation
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
  PUBLIC ${PROJECT_SOURCE_DIR}
)

target_link_libraries(tls-test-validation PUBLIC syslog-ng)

