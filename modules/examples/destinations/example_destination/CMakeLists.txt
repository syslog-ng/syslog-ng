module_switch(ENABLE_EXAMPLE_DESTINATION "Enable example_destination")
if (NOT ENABLE_EXAMPLE_DESTINATION)
  return()
endif()

set(example_destination_SOURCES
    "example_destination-parser.h"
    "example_destination-plugin.c"
    "example_destination-parser.c"
    "example_destination.h"
    "example_destination.c"
    "example_destination_worker.h"
    "example_destination_worker.c"
)

add_module(
  TARGET example_destination
  GRAMMAR example_destination-grammar
  SOURCES ${example_destination_SOURCES}
  LIBRARY_TYPE STATIC # STATIC: Built as part of a larger libexamples module
)
