set(HTTP_TEST_SLOTS_HEADERS
    "${CMAKE_CURRENT_BINARY_DIR}/http-test-slots-grammar.h"
    "http-test-slots-parser.h"
    "http-test-slots.h"
)

set(HTTP_TEST_SLOTS_SOURCES
    "${CMAKE_CURRENT_BINARY_DIR}/http-test-slots-grammar.c"
    "http-test-slots-plugin.c"
    "http-test-slots-parser.c"
    "http-test-slots.c"
)

generate_y_from_ym(modules/http-test-slots/http-test-slots-grammar)

module_switch(ENABLE_HTTP_TEST_SLOTS "Enable http-test-slots module")

if (ENABLE_HTTP_TEST_SLOTS)
  bison_target(HttpTestSlotsGrammar
      ${CMAKE_CURRENT_BINARY_DIR}/http-test-slots-grammar.y
      ${CMAKE_CURRENT_BINARY_DIR}/http-test-slots-grammar.c
      COMPILE_FLAGS ${BISON_FLAGS})

  add_library(http-test-slots MODULE ${HTTP_TEST_SLOTS_SOURCES})
  target_include_directories(http-test-slots
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
  )
  target_link_libraries(http-test-slots PRIVATE syslog-ng)

  install(TARGETS http-test-slots LIBRARY DESTINATION lib/syslog-ng/ COMPONENT http-test-slots)
endif()
