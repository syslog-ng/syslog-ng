lib_transport_tests_TESTS		 = \
	lib/transport/tests/test_aux_data \
	lib/transport/tests/test_transport_factory_id

check_PROGRAMS				+= ${lib_transport_tests_TESTS}

lib_transport_tests_test_aux_data_CFLAGS  = $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/transport/tests
lib_transport_tests_test_aux_data_LDADD	 = $(TEST_LDADD)
lib_transport_tests_test_aux_data_SOURCES = 			\
	lib/transport/tests/test_aux_data.c

lib_transport_tests_test_transport_factory_id_CFLAGS  = $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/transport/tests
lib_transport_tests_test_transport_factory_id_LDADD	 = $(TEST_LDADD)
lib_transport_tests_test_transport_factory_id_SOURCES = 			\
	lib/transport/tests/test_transport_factory_id.c
