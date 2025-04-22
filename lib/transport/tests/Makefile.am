lib_transport_tests_TESTS		 = \
	lib/transport/tests/test_aux_data \
	lib/transport/tests/test_transport \
	lib/transport/tests/test_transport_stack \
	lib/transport/tests/test_transport_haproxy

EXTRA_DIST += lib/transport/tests/CMakeLists.txt

check_PROGRAMS				+= ${lib_transport_tests_TESTS}

lib_transport_tests_test_aux_data_CFLAGS  = $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/transport/tests
lib_transport_tests_test_aux_data_LDADD	 = $(TEST_LDADD)
lib_transport_tests_test_aux_data_SOURCES = 			\
	lib/transport/tests/test_aux_data.c

lib_transport_tests_test_transport_CFLAGS  = $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/transport/tests
lib_transport_tests_test_transport_LDADD	 = $(TEST_LDADD)
lib_transport_tests_test_transport_SOURCES = 			\
	lib/transport/tests/test_transport.c

lib_transport_tests_test_transport_stack_CFLAGS  = $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/transport/tests
lib_transport_tests_test_transport_stack_LDADD	 = $(TEST_LDADD)
lib_transport_tests_test_transport_stack_SOURCES = 			\
	lib/transport/tests/test_transport_stack.c

lib_transport_tests_test_transport_haproxy_CFLAGS  = $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/transport/tests
lib_transport_tests_test_transport_haproxy_LDADD	 = $(TEST_LDADD)
lib_transport_tests_test_transport_haproxy_SOURCES = \
	lib/transport/tests/test_transport_haproxy.c
