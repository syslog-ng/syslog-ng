add_unit_test(LIBTEST CRITERION
  TARGET test_geoip_parser
  INCLUDES "${GEOIP2_INCLUDE_DIR}"
  DEPENDS geoip2-plugin
  SOURCES test_geoip_parser.c)
