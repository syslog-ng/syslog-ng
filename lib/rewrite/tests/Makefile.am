lib_rewrite_tests_TESTS = \
    lib/rewrite/tests/test_rewrite \
    lib/rewrite/tests/test_set_level

EXTRA_DIST += lib/rewrite/tests/CMakeLists.txt

check_PROGRAMS  +=  ${lib_rewrite_tests_TESTS}

lib_rewrite_tests_test_rewrite_CFLAGS = $(TEST_CFLAGS)
lib_rewrite_tests_test_rewrite_LDADD = $(TEST_LDADD)    \
    $(PREOPEN_SYSLOGFORMAT)
lib_rewrite_tests_test_rewrite_SOURCES =    \
    lib/rewrite/tests/test_rewrite.c

lib_rewrite_tests_test_set_level_CFLAGS = $(TEST_CFLAGS)
lib_rewrite_tests_test_set_level_LDADD = $(TEST_LDADD)
lib_rewrite_tests_test_set_level_SOURCES =    \
    lib/rewrite/tests/test_set_level.c
