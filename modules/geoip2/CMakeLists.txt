find_package(LibMaxMindDB)
if (LIBMAXMINDDB_FOUND)
  option(ENABLE_GEOIP2 "Enable geoip2 parser and template function" ON)
else()
  option(ENABLE_GEOIP2 "Enable geoip2 parser and template function" OFF)
endif()

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
  ${CMAKE_CURRENT_BINARY_DIR}/geoip-parser-grammar.c
)

set(GEOIP2_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

generate_y_from_ym(modules/geoip2/geoip-parser-grammar)
bison_target(GeoIP2Grammar
  ${CMAKE_CURRENT_BINARY_DIR}/geoip-parser-grammar.y
  ${CMAKE_CURRENT_BINARY_DIR}/geoip-parser-grammar.c
  COMPILE_FLAGS ${BISON_FLAGS})

add_library(geoip2-plugin SHARED ${GEOIP2_SOURCES})
target_link_libraries (geoip2-plugin PUBLIC ${LIBMAXMINDDB_LIBRARY})
target_include_directories (geoip2-plugin SYSTEM PRIVATE ${LIBMAXMINDDB_INCLUDE_DIR})
target_include_directories (geoip2-plugin PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(geoip2-plugin PRIVATE syslog-ng)

install(TARGETS geoip2-plugin LIBRARY DESTINATION lib/syslog-ng/ COMPONENT geoip2)

add_test_subdirectory(tests)
