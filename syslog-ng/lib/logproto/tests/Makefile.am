lib_logproto_tests_TESTS		 = \
	lib/logproto/tests/test_logproto   \
	lib/logproto/tests/test_findeom

EXTRA_DIST += lib/logproto/tests/CMakeLists.txt

check_PROGRAMS				+= ${lib_logproto_tests_TESTS}

lib_logproto_tests_test_logproto_CFLAGS  = $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/logproto/tests
lib_logproto_tests_test_logproto_LDADD	 = $(TEST_LDADD)
lib_logproto_tests_test_logproto_SOURCES = 			\
	lib/logproto/tests/test_logproto.c 			\
	lib/logproto/tests/test-server-options.c		\
	lib/logproto/tests/test-record-server.c			\
	lib/logproto/tests/test-text-server.c			\
	lib/logproto/tests/test-dgram-server.c			\
	lib/logproto/tests/test-framed-server.c			\
	lib/logproto/tests/test-indented-multiline-server.c	\
	lib/logproto/tests/test-regexp-multiline-server.c

lib_logproto_tests_test_findeom_CFLAGS	= \
	$(TEST_CFLAGS) \
	-I${top_srcdir}/libtest
lib_logproto_tests_test_findeom_LDADD	= \
	${top_builddir}/lib/libsyslog-ng.la \
	${top_builddir}/libtest/libsyslog-ng-test.a \
	$(TEST_LDADD)
lib_logproto_tests_test_findeom_SOURCES = \
	lib/logproto/tests/test_findeom.c
