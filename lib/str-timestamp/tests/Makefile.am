lib_str_timestamp_tests_TESTS		= \
	lib/str-timestamp/tests/test_parse_timestamp

check_PROGRAMS				+= ${lib_str_timestamp_tests_TESTS}

lib_str_timestamp_tests_test_parse_timestamp_SOURCES	= lib/str-timestamp/tests/test_parse_timestamp.c
lib_str_timestamp_tests_test_parse_timestamp_CFLAGS	= $(TEST_CFLAGS) -I$(top_srcdir)/lib/str-timestamp
lib_str_timestamp_tests_test_parse_timestamp_LDADD	= \
	$(TEST_LDADD)

EXTRA_DIST += lib/str-timestamp/tests/CMakeLists.txt
