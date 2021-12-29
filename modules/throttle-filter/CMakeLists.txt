set(THROTTLE_FILTER_SOURCES
    throttle-filter-parser.h
    throttle-filter-plugin.c
    throttle-filter-parser.c
    throttle-filter.c
    throttle-filter.h
)

add_module(
  TARGET throttle_filter
  GRAMMAR throttle-filter-grammar
  SOURCES ${THROTTLE_FILTER_SOURCES}
)
