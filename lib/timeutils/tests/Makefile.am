lib_timeutils_tests_TESTS		= \
	lib/timeutils/tests/test_parse_timestamp	\
	lib/timeutils/tests/test_timeutils

check_PROGRAMS				+= ${lib_timeutils_tests_TESTS}

lib_timeutils_tests_test_parse_timestamp_SOURCES	= lib/timeutils/tests/test_parse_timestamp.c
lib_timeutils_tests_test_parse_timestamp_CFLAGS	= $(TEST_CFLAGS) -I$(top_srcdir)/lib/timeutils
lib_timeutils_tests_test_parse_timestamp_LDADD	= \
	$(TEST_LDADD)

lib_timeutils_tests_test_timeutils_SOURCES	= lib/timeutils/tests/test_timeutils.c
lib_timeutils_tests_test_timeutils_CFLAGS	= $(TEST_CFLAGS) -I$(top_srcdir)/lib/timeutils
lib_timeutils_tests_test_timeutils_LDADD	= \
	$(TEST_LDADD)

EXTRA_DIST += lib/timeutils/tests/CMakeLists.txt
