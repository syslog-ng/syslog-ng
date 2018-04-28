add_unit_test(TARGET test_filters DEPENDS syslogformat)

add_unit_test(LIBTEST TARGET test_filters_in_list DEPENDS syslogformat)
target_compile_definitions(test_filters_in_list PRIVATE TOP_SRCDIR="${PROJECT_SOURCE_DIR}")

add_unit_test(LIBTEST TARGET test_filters_netmask6)

add_unit_test(CRITERION TARGET test_filters_statistics DEPENDS syslogformat)
