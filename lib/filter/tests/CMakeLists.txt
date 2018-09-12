add_unit_test(TARGET test_filters DEPENDS syslogformat)

add_unit_test(LIBTEST TARGET test_filters_in_list DEPENDS syslogformat)

add_unit_test(LIBTEST CRITERION TARGET test_filters_netmask6)

add_unit_test(CRITERION TARGET test_filters_statistics DEPENDS syslogformat)

add_unit_test(CRITERION TARGET test_filter_call)
