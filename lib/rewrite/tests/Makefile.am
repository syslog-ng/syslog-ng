lib_rewrite_tests_TESTS = \
    lib/rewrite/tests/test_rewrite

EXTRA_DIST += lib/rewrite/tests/CMakeLists.txt

check_PROGRAMS  +=  ${lib_rewrite_tests_TESTS}

lib_rewrite_tests_test_rewrite_CFLAGS = $(TEST_CFLAGS)

lib_rewrite_tests_test_rewrite_LDADD = $(TEST_LDADD)    \
    $(PREOPEN_SYSLOGFORMAT)

lib_rewrite_tests_test_rewrite_SOURCES =    \
    lib/rewrite/tests/test_rewrite.c
