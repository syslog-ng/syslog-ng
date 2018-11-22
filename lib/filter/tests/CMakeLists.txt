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

set(TEST_FILTERS_REGEXP_SOURCE
  test_filters_regexp.c
  test_filters_common.c
  test_filters_common.h
  )

set(TEST_FILTERS_FOP_CMP_SOURCE
  test_filters_fop_cmp.c
  test_filters_common.c
  test_filters_common.h
  )

set(TEST_FILTERS_NETMASK_SOURCE
  test_filters_netmask.c
  test_filters_common.c
  test_filters_common.h
  )

set(TEST_FILTERS_NETMASK6_SOURCE
  test_filters_netmask6.c
  test_filters_common.c
  test_filters_common.h
  )

add_unit_test(CRITERION TARGET test_filters_facility SOURCES ${TEST_FILTERS_FACILITY_SOURCE} DEPENDS syslogformat)
add_unit_test(CRITERION TARGET test_filters_level_new SOURCES ${TEST_FILTERS_LEVEL_NEW_SOURCE} DEPENDS syslogformat)
add_unit_test(CRITERION TARGET test_filters_regexp SOURCES ${TEST_FILTERS_REGEXP_SOURCE} DEPENDS syslogformat)
add_unit_test(CRITERION TARGET test_filters_fop_cmp SOURCES ${TEST_FILTERS_FOP_CMP_SOURCE} DEPENDS syslogformat)
add_unit_test(CRITERION TARGET test_filters_netmask SOURCES ${TEST_FILTERS_NETMASK_SOURCE} DEPENDS syslogformat)

add_unit_test(LIBTEST TARGET test_filters_in_list DEPENDS syslogformat)

add_unit_test(CRITERION TARGET test_filters_netmask6 SOURCES ${TEST_FILTERS_NETMASK6_SOURCE} DEPENDS syslogformat)

add_unit_test(CRITERION TARGET test_filters_statistics DEPENDS syslogformat)

add_unit_test(CRITERION TARGET test_filter_call)
