add_unit_test(LIBTEST
  TARGET test_geoip2_parser
  INCLUDES "${GEOIP2_INCLUDE_DIR}"
  DEPENDS geoip2-plugin
  SOURCES test_geoip_parser)
target_compile_definitions(test_geoip2_parser PRIVATE TOP_SRCDIR="${CMAKE_SOURCE_DIR}")
