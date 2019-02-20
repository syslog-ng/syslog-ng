lib_timeutils_tests_TESTS		= \
	lib/timeutils/tests/test_scan_timestamp	\
	lib/timeutils/tests/test_conv		\
	lib/timeutils/tests/test_wallclocktime	\
	lib/timeutils/tests/test_unixtime

check_PROGRAMS				+= ${lib_timeutils_tests_TESTS}

lib_timeutils_tests_test_conv_SOURCES	= lib/timeutils/tests/test_conv.c
lib_timeutils_tests_test_conv_CFLAGS	= $(TEST_CFLAGS) -I$(top_srcdir)/lib/timeutils
lib_timeutils_tests_test_conv_LDADD	= \
	$(TEST_LDADD)

lib_timeutils_tests_test_scan_timestamp_SOURCES	= lib/timeutils/tests/test_scan-timestamp.c
lib_timeutils_tests_test_scan_timestamp_CFLAGS	= $(TEST_CFLAGS) -I$(top_srcdir)/lib/timeutils
lib_timeutils_tests_test_scan_timestamp_LDADD	= \
	$(TEST_LDADD)

lib_timeutils_tests_test_wallclocktime_SOURCES	= lib/timeutils/tests/test_wallclocktime.c
lib_timeutils_tests_test_wallclocktime_CFLAGS	= $(TEST_CFLAGS) -I$(top_srcdir)/lib/timeutils
lib_timeutils_tests_test_wallclocktime_LDADD	= \
	$(TEST_LDADD)

lib_timeutils_tests_test_unixtime_SOURCES	= lib/timeutils/tests/test_unixtime.c
lib_timeutils_tests_test_unixtime_CFLAGS	= $(TEST_CFLAGS) -I$(top_srcdir)/lib/timeutils
lib_timeutils_tests_test_unixtime_LDADD	= \
	$(TEST_LDADD)

EXTRA_DIST += lib/timeutils/tests/CMakeLists.txt
