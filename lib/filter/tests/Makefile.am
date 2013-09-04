lib_filter_tests_TESTS		 = \
	lib/filter/tests/test_filters				\
	lib/filter/tests/test_filters_in_list

check_PROGRAMS				+= ${lib_filter_tests_TESTS}

lib_filter_tests_test_filters_CFLAGS  = $(TEST_CFLAGS) 		\
	-I${top_srcdir}/lib/filter/tests
lib_filter_tests_test_filters_LDADD	 = $(TEST_LDADD)	\
	$(PREOPEN_SYSLOGFORMAT)
lib_filter_tests_test_filters_SOURCES = 			\
	lib/filter/tests/test_filters.c

lib_filter_tests_test_filters_in_list_CFLAGS	= $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/filter/tests
lib_filter_tests_test_filters_in_list_LDADD	= $(TEST_LDADD)  \
	$(PREOPEN_SYSLOGFORMAT)

EXTRA_DIST				+=	\
	lib/filter/tests/test.list
