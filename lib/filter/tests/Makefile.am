lib_filter_tests_TESTS		 =              \
    lib/filter/tests/test_filters               \
    lib/filter/tests/test_filters_in_list

if ENABLE_IPV6
lib_filter_tests_TESTS += lib/filter/tests/test_filters_netmask6
endif

check_PROGRAMS				+= ${lib_filter_tests_TESTS}

lib_filter_tests_test_filters_CFLAGS  = $(TEST_CFLAGS) 		\
	-I${top_srcdir}/lib/filter/tests
lib_filter_tests_test_filters_LDADD	 = $(TEST_LDADD)	\
	$(PREOPEN_SYSLOGFORMAT)
lib_filter_tests_test_filters_SOURCES = 			\
	lib/filter/tests/test_filters.c

lib_filter_tests_test_filters_in_list_CFLAGS     = $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/filter/tests
lib_filter_tests_test_filters_in_list_LDADD      = $(TEST_LDADD)  \
	$(PREOPEN_SYSLOGFORMAT)

if ENABLE_IPV6
lib_filter_tests_test_filters_netmask6_CFLAGS    = $(TEST_CFLAGS) \
    -I${top_srcdir}/lib/filter/tests
lib_filter_tests_test_filters_netmask6_LDADD     = $(TEST_LDADD)  \
    $(PREOPEN_SYSLOGFORMAT)
endif

lib_filter_tests_TESTS += lib/filter/tests/test_filters_statistics

lib_filter_tests_test_filters_statistics_CFLAGS  = $(TEST_CFLAGS) \
    -I${top_srcdir}/lib/filter/tests
lib_filter_tests_test_filters_statistics_LDADD     = $(TEST_LDADD)  \
    $(PREOPEN_SYSLOGFORMAT)

include lib/filter/tests/filters-in-list/Makefile.am
