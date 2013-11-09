lib_compat_tests_TESTS		 = \
	lib/compat/tests/test_strtok_r

check_PROGRAMS				+= ${lib_compat_tests_TESTS}

lib_compat_tests_test_strtok_r_CFLAGS  = $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/compat/tests
lib_compat_tests_test_strtok_r_LDADD	 = $(TEST_LDADD)
lib_compat_tests_test_strtok_r_SOURCES = \
	lib/compat/tests/test_strtok_r.c
