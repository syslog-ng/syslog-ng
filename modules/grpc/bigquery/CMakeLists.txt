module_switch(ENABLE_BIGQUERY "Enable bigquery")
if (NOT ENABLE_BIGQUERY)
  return()
endif()

set(BIGQUERY_SOURCES
    bigquery-parser.h
    bigquery-plugin.c
    bigquery-parser.c
)

add_module(
  TARGET bigquery
  GRAMMAR bigquery-grammar
  SOURCES ${BIGQUERY_SOURCES}
)
