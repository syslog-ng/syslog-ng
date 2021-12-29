module_switch(ENABLE_throttle_filter "Enable throttle-filter")
if (NOT ENABLE_throttle_filter)
  return()
endif()

set(throttle_filter_SOURCES
    "throttle-filter-parser.h"
    "throttle-filter-plugin.c"
    "throttle-filter-parser.c"
)

add_module(
  TARGET throttle_filter
  GRAMMAR throttle-filter-grammar
  SOURCES ${throttle-filter_SOURCES}
)

