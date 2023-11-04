lib_filterx_tests_TESTS		 =              \
		lib/filterx/tests/test_filterx_object	\
		lib/filterx/tests/test_filterx_expr

#EXTRA_DIST += lib/filterx/tests/CMakeLists.txt

check_PROGRAMS				+= ${lib_filterx_tests_TESTS}

lib_filterx_tests_test_filterx_object_CFLAGS  = $(TEST_CFLAGS)
lib_filterx_tests_test_filterx_object_LDADD   = $(TEST_LDADD)

lib_filterx_tests_test_filterx_expr_CFLAGS  = $(TEST_CFLAGS)
lib_filterx_tests_test_filterx_expr_LDADD   = $(TEST_LDADD) $(PREOPEN_SYSLOGFORMAT)
