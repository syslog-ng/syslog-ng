set(RATE_LIMIT_FILTER_SOURCES
    rate-limit-parser.h
    rate-limit-plugin.c
    rate-limit-parser.c
    rate-limit.c
    rate-limit.h
)

add_module(
  TARGET rate_limit_filter
  GRAMMAR rate-limit-grammar
  SOURCES ${RATE_LIMIT_FILTER_SOURCES}
)
set_target_properties(rate_limit_filter PROPERTIES OUTPUT_NAME "rate-limit-filter")
