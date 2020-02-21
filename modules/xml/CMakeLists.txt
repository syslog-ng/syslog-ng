module_switch(ENABLE_XML "Enable xml")

if (NOT ENABLE_XML)
  return()
endif()

set(xml_SOURCES
    "xml-parser.h"
    "xml.h"
    "xml-plugin.c"
    "xml-parser.c"
    "xml.c"
)


add_module(
  TARGET xml
  GRAMMAR xml-grammar
  SOURCES ${xml_SOURCES}
)

add_test_subdirectory(tests)
