lib_stats_tests_TESTS		 = \
	lib/stats/tests/test_stats_cluster

check_PROGRAMS				+= ${lib_stats_tests_TESTS}

lib_stats_tests_test_stats_cluster_CFLAGS	= $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/stats/tests
lib_stats_tests_test_stats_cluster_LDADD	= $(TEST_LDADD)
lib_stats_tests_test_stats_cluster_SOURCES	= 		\
	lib/stats/tests/test_stats_cluster.c
