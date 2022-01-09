lib_logmsg_tests_TESTS =                    \
 lib/logmsg/tests/test_logmsg_serialize     \
 lib/logmsg/tests/test_timestamp_serialize  \
 lib/logmsg/tests/test_tags		    \
 lib/logmsg/tests/test_type_hints

EXTRA_DIST += lib/logmsg/tests/CMakeLists.txt \
	lib/logmsg/tests/messages/syslog-ng-3.17.1-msg.h \
	lib/logmsg/tests/messages/syslog-ng-3.18.1-msg.h \
	lib/logmsg/tests/messages/syslog-ng-3.21.1-msg.h \
	lib/logmsg/tests/messages/syslog-ng-3.25.1-msg.h \
	lib/logmsg/tests/messages/syslog-ng-3.26.1-msg.h \
	lib/logmsg/tests/messages/syslog-ng-3.28.1-msg.h \
	lib/logmsg/tests/messages/syslog-ng-3.29.1-msg.h \
	lib/logmsg/tests/messages/syslog-ng-3.30.1-msg.h \
	lib/logmsg/tests/messages/syslog-ng-pe-6.0-msg.h


check_PROGRAMS       += ${lib_logmsg_tests_TESTS}

lib_logmsg_tests_test_timestamp_serialize_CFLAGS = $(TEST_CFLAGS)
lib_logmsg_tests_test_timestamp_serialize_LDADD  = $(TEST_LDADD)

lib_logmsg_tests_test_logmsg_serialize_CFLAGS = $(TEST_CFLAGS)
lib_logmsg_tests_test_logmsg_serialize_LDADD  = $(TEST_LDADD) $(PREOPEN_SYSLOGFORMAT)

lib_logmsg_tests_test_tags_CFLAGS	      = $(TEST_CFLAGS)
lib_logmsg_tests_test_tags_LDADD	      = $(TEST_LDADD)

lib_logmsg_tests_test_type_hints_CFLAGS = $(TEST_CFLAGS)
lib_logmsg_tests_test_type_hints_LDADD	= $(TEST_LDADD)


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

.PHONY: dump-logmsg

if ENABLE_TESTING
noinst_PROGRAMS +=				\
	lib/logmsg/tests/dump_logmsg

lib_logmsg_tests_dump_logmsg_CFLAGS = $(TEST_CFLAGS)
lib_logmsg_tests_dump_logmsg_LDADD = $(TEST_LDADD) $(PREOPEN_SYSLOGFORMAT)

dump-logmsg: lib/logmsg/tests/dump_logmsg
	DIR=lib/logmsg/tests/messages/ && \
	mkdir -p $${DIR} && \
	lib/logmsg/tests/dump_logmsg $(VERSION) > $${DIR}/syslog-ng-$(VERSION)-msg.h

endif
