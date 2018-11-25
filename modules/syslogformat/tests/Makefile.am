modules_syslogformat_tests_TESTS		= \
	modules/syslogformat/tests/test_parse_timestamp

check_PROGRAMS				+= ${modules_syslogformat_tests_TESTS}

modules_syslogformat_tests_test_parse_timestamp_SOURCES	= modules/syslogformat/tests/test_parse_timestamp.c
modules_syslogformat_tests_test_parse_timestamp_CFLAGS	= $(TEST_CFLAGS) -I$(top_srcdir)/modules/syslogformat
modules_syslogformat_tests_test_parse_timestamp_LDFLAGS	= $(LDFLAGS_FORCE_FLAT_NAMESPACE)
modules_syslogformat_tests_test_parse_timestamp_LDADD	= \
	$(TEST_LDADD)			\
	$(PREOPEN_SYSLOGFORMAT)

EXTRA_DIST += modules/syslogformat/tests/CMakeLists.txt
