set(xml_HEADERS
    "xml-parser.h"
    "xml.h"
    "xml-scanner.h"
    "${CMAKE_CURRENT_BINARY_DIR}/xml-grammar.h"
)

set(xml_SOURCES
    "xml-plugin.c"
    "xml-parser.c"
    "xml.c"
    "xml-scanner.c"
    "${CMAKE_CURRENT_BINARY_DIR}/xml-grammar.c"
)

generate_y_from_ym(modules/xml/xml-grammar)

bison_target(xmlGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/xml-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/xml-grammar.c
COMPILE_FLAGS ${BISON_FLAGS})

option(ENABLE_XML "Enable xml" ON)

if (ENABLE_XML)
  add_library(xml SHARED ${xml_SOURCES})
  target_include_directories(xml
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
  )
  target_link_libraries(xml PRIVATE syslog-ng)

  install(TARGETS xml
  LIBRARY DESTINATION lib/syslog-ng/
  COMPONENT xml)

  add_test_subdirectory(tests)
endif()
