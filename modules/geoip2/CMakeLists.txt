find_package(LibMaxMindDB)

module_switch(ENABLE_GEOIP2 "Enable geoip2 parser and template function" LIBMAXMINDDB_FOUND)

if (NOT ENABLE_GEOIP2)
  return ()
endif ()

if (NOT LIBMAXMINDDB_FOUND)
  message(FATAL_ERROR "GeIP2 module is enabled but libmaxminddb not found.")
endif ()

set(GEOIP2_SOURCES
  geoip-parser.c
  geoip-parser-parser.c
  geoip-plugin.c
  maxminddb-helper.c
)

add_module(
  TARGET geoip2-plugin
  GRAMMAR geoip-parser-grammar
  INCLUDES ${LIBMAXMINDDB_INCLUDE_DIR}
  DEPENDS ${LIBMAXMINDDB_LIBRARY}
  SOURCES ${GEOIP2_SOURCES}
)

add_test_subdirectory(tests)
