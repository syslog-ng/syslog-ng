add_unit_test(TARGET test_timer_wheel INCLUDES ${PATTERNDB_INCLUDE_DIR} DEPENDS patterndb)
add_unit_test(TARGET test_patternize INCLUDES ${PATTERNDB_INCLUDE_DIR} DEPENDS patterndb syslogformat)
add_unit_test(LIBTEST TARGET test_patterndb INCLUDES ${PATTERNDB_INCLUDE_DIR}
  DEPENDS patterndb basicfuncs syslogformat)
add_unit_test(TARGET test_radix INCLUDES ${PATTERNDB_INCLUDE_DIR} DEPENDS patterndb)
target_compile_options(test_radix PRIVATE "-Wno-error=pointer-sign")
add_unit_test(LIBTEST TARGET test_parsers INCLUDES ${PATTERNDB_INCLUDE_DIR})
target_compile_options(test_parsers PRIVATE "-Wno-error=pointer-sign")
