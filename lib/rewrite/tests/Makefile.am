lib_rewrite_tests_TESTS = \
    lib/rewrite/tests/test_rewrite \
    lib/rewrite/tests/test_set_pri \
    lib/rewrite/tests/test_set_severity \
    lib/rewrite/tests/test_set_facility

EXTRA_DIST += lib/rewrite/tests/CMakeLists.txt

check_PROGRAMS  +=  ${lib_rewrite_tests_TESTS}

lib_rewrite_tests_test_rewrite_CFLAGS = $(TEST_CFLAGS)
lib_rewrite_tests_test_rewrite_LDADD = $(TEST_LDADD)    \
    $(PREOPEN_SYSLOGFORMAT)
lib_rewrite_tests_test_rewrite_SOURCES =    \
    lib/rewrite/tests/test_rewrite.c

lib_rewrite_tests_test_set_severity_CFLAGS = $(TEST_CFLAGS)
lib_rewrite_tests_test_set_severity_LDADD = $(TEST_LDADD)
lib_rewrite_tests_test_set_severity_SOURCES =    \
    lib/rewrite/tests/test_set_severity.c

lib_rewrite_tests_test_set_facility_CFLAGS = $(TEST_CFLAGS)
lib_rewrite_tests_test_set_facility_LDADD = $(TEST_LDADD)
lib_rewrite_tests_test_set_facility_SOURCES =    \
    lib/rewrite/tests/test_set_facility.c

lib_rewrite_tests_test_set_pri_CFLAGS = $(TEST_CFLAGS)
lib_rewrite_tests_test_set_pri_LDADD = $(TEST_LDADD)
lib_rewrite_tests_test_set_pri_SOURCES =    \
    lib/rewrite/tests/test_set_pri.c
