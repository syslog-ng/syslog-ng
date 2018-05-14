libtest_tests_TESTS			= \
	libtest/tests/test_mock_transport

check_PROGRAMS					+= ${libtest_tests_TESTS}

libtest_tests_test_mock_transport_CFLAGS	= $(TEST_CFLAGS) -I$(top_builddir)/libtest
libtest_tests_test_mock_transport_LDADD	= \
	$(TEST_LDADD)
