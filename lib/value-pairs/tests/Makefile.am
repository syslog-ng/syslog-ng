if ENABLE_CRITERION

lib_value_pairs_tests_TESTS			=  \
	lib/value-pairs/tests/test_value_pairs     \
	lib/value-pairs/tests/test_value_pairs_walk

check_PROGRAMS				+= \
	${lib_value_pairs_tests_TESTS}

lib_value_pairs_tests_test_value_pairs_LDADD	= $(TEST_LDADD)
lib_value_pairs_tests_test_value_pairs_CFLAGS	= $(TEST_CFLAGS)
lib_value_pairs_tests_test_value_pairs_LDFLAGS	= $(PREOPEN_SYSLOGFORMAT)

lib_value_pairs_tests_test_value_pairs_walk_LDADD = $(TEST_LDADD)
lib_value_pairs_tests_test_value_pairs_walk_CFLAGS = $(TEST_CFLAGS)

endif
