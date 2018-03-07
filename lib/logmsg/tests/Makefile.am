lib_logmsg_tests_TESTS =                       \
 lib/logmsg/tests/test_logmsg_serialize     \
 lib/logmsg/tests/test_timestamp_serialize  \
 lib/logmsg/tests/test_tags


check_PROGRAMS       += ${lib_logmsg_tests_TESTS}

lib_logmsg_tests_test_timestamp_serialize_CFLAGS = $(TEST_CFLAGS)
lib_logmsg_tests_test_timestamp_serialize_LDADD  = $(TEST_LDADD)

lib_logmsg_tests_test_logmsg_serialize_CFLAGS = $(TEST_CFLAGS)
lib_logmsg_tests_test_logmsg_serialize_LDADD  = $(TEST_LDADD) $(PREOPEN_SYSLOGFORMAT)

lib_logmsg_tests_test_tags_CFLAGS	      = $(TEST_CFLAGS)
lib_logmsg_tests_test_tags_LDADD	      = $(TEST_LDADD)

lib_logmsg_tests_TESTS +=				\
	lib/logmsg/tests/test_nvtable			\
	lib/logmsg/tests/test_gsockaddr_serialize	\
	lib/logmsg/tests/test_log_message \
	lib/logmsg/tests/test_logmsg_ack \
	lib/logmsg/tests/test_nvhandle_desc_array

lib_logmsg_tests_test_nvtable_CFLAGS			= $(TEST_CFLAGS)
lib_logmsg_tests_test_nvtable_LDADD			= $(TEST_LDADD)

lib_logmsg_tests_test_gsockaddr_serialize_CFLAGS	= $(TEST_CFLAGS)
lib_logmsg_tests_test_gsockaddr_serialize_LDADD		= $(TEST_LDADD)

lib_logmsg_tests_test_log_message_CFLAGS		= $(TEST_CFLAGS)
lib_logmsg_tests_test_log_message_LDADD			= $(TEST_LDADD)

lib_logmsg_tests_test_logmsg_ack_LDADD = $(TEST_LDADD)
lib_logmsg_tests_test_logmsg_ack_CFLAGS = $(TEST_CFLAGS)

lib_logmsg_tests_test_nvhandle_desc_array_LDADD = $(TEST_LDADD)
lib_logmsg_tests_test_nvhandle_desc_array_CFLAGS = $(TEST_CFLAGS)
