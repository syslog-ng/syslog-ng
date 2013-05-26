lib_filter_tests_TESTS		 = \
	lib/filter/tests/test_filters

check_PROGRAMS				+= ${lib_filter_tests_TESTS}

lib_filter_tests_test_filters_CFLAGS  = $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/filter/tests
lib_filter_tests_test_filters_LDADD	 = $(TEST_LDADD)
lib_filter_tests_test_filters_SOURCES = 			\
	lib/filter/tests/test_filters.c
