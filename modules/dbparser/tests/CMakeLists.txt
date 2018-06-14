add_unit_test(TARGET test_timer_wheel DEPENDS patterndb)
add_unit_test(TARGET test_patternize DEPENDS patterndb syslogformat)
add_unit_test(CRITERION LIBTEST TARGET test_patterndb DEPENDS patterndb basicfuncs syslogformat)
add_unit_test(TARGET test_radix DEPENDS patterndb)
target_compile_options(test_radix PRIVATE "-Wno-error=pointer-sign")

# test_parsers includes a .c file
add_unit_test(CRITERION TARGET test_parsers INCLUDES ${PATTERNDB_INCLUDE_DIR})
target_compile_options(test_parsers PRIVATE "-Wno-error=pointer-sign")
