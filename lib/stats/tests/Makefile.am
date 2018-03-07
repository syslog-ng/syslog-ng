lib_stats_tests_TESTS		 = \
	lib/stats/tests/test_stats_cluster

check_PROGRAMS				+= ${lib_stats_tests_TESTS}

lib_stats_tests_test_stats_cluster_CFLAGS	= $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/stats/tests
lib_stats_tests_test_stats_cluster_LDADD	= $(TEST_LDADD)
lib_stats_tests_test_stats_cluster_SOURCES	= 		\
	lib/stats/tests/test_stats_cluster.c

stats_test_extra_modules			= \
	$(PREOPEN_SYSLOGFORMAT)

lib_stats_tests_TESTS		+= \
	lib/stats/tests/test_stats_query \
	lib/stats/tests/test_dynamic_ctr_reg

lib_stats_tests_test_stats_query_CFLAGS	= $(TEST_CFLAGS)
lib_stats_tests_test_stats_query_LDADD	= \
	$(TEST_LDADD) $(stats_test_extra_modules)

lib_stats_tests_test_dynamic_ctr_reg_CFLAGS = $(TEST_CFLAGS)
lib_stats_tests_test_dynamic_ctr_reg_LDADD = \
	$(TEST_LDADD) $(stats_test_extra_modules)
