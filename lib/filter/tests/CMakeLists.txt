set(TEST_FILTERS_SOURCE
  test_filters.c
  test_filters_common.c
  test_filters_common.h
  )

set(TEST_FILTERS_FACILITY_SOURCE
  test_filters_facility.c
  test_filters_common.c
  test_filters_common.h
  )

set(TEST_FILTERS_LEVEL_NEW_SOURCE
  test_filters_level_new.c
  test_filters_common.c
  test_filters_common.h
  )

add_unit_test(CRITERION TARGET test_filters SOURCES ${TEST_FILTERS_SOURCE} DEPENDS syslogformat)
add_unit_test(CRITERION TARGET test_filters_facility SOURCES ${TEST_FILTERS_FACILITY_SOURCE} DEPENDS syslogformat)
add_unit_test(CRITERION TARGET test_filters_level_new SOURCES ${TEST_FILTERS_LEVEL_NEW_SOURCE} DEPENDS syslogformat)

add_unit_test(LIBTEST TARGET test_filters_in_list DEPENDS syslogformat)

add_unit_test(LIBTEST CRITERION TARGET test_filters_netmask6)

add_unit_test(CRITERION TARGET test_filters_statistics DEPENDS syslogformat)

add_unit_test(CRITERION TARGET test_filter_call)
