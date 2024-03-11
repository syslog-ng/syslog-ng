lib_filterx_tests_TESTS		 =              \
		lib/filterx/tests/test_object_primitive	\
		lib/filterx/tests/test_object_message	\
		lib/filterx/tests/test_object_datetime	\
		lib/filterx/tests/test_object_json	\
		lib/filterx/tests/test_object_null	\
		lib/filterx/tests/test_object_string	\
		lib/filterx/tests/test_filterx_expr	\
		lib/filterx/tests/test_expr_comparison \
		lib/filterx/tests/test_expr_condition

EXTRA_DIST += lib/filterx/tests/CMakeLists.txt \
	lib/filterx/tests/filterx-lib.h

check_PROGRAMS				+= ${lib_filterx_tests_TESTS}

lib_filterx_tests_test_object_primitive_CFLAGS  = $(TEST_CFLAGS)
lib_filterx_tests_test_object_primitive_LDADD   = $(TEST_LDADD) $(JSON_LIBS)

lib_filterx_tests_test_object_message_CFLAGS  = $(TEST_CFLAGS)
lib_filterx_tests_test_object_message_LDADD   = $(TEST_LDADD) $(JSON_LIBS)

lib_filterx_tests_test_object_datetime_CFLAGS  = $(TEST_CFLAGS)
lib_filterx_tests_test_object_datetime_LDADD   = $(TEST_LDADD) $(JSON_LIBS)

lib_filterx_tests_test_object_json_CFLAGS  = $(TEST_CFLAGS)
lib_filterx_tests_test_object_json_LDADD   = $(TEST_LDADD) $(JSON_LIBS)

lib_filterx_tests_test_object_null_CFLAGS  = $(TEST_CFLAGS)
lib_filterx_tests_test_object_null_LDADD   = $(TEST_LDADD) $(JSON_LIBS)

lib_filterx_tests_test_object_string_CFLAGS  = $(TEST_CFLAGS)
lib_filterx_tests_test_object_string_LDADD   = $(TEST_LDADD) $(JSON_LIBS)

lib_filterx_tests_test_filterx_expr_CFLAGS  = $(TEST_CFLAGS)
lib_filterx_tests_test_filterx_expr_LDADD   = $(TEST_LDADD) $(PREOPEN_SYSLOGFORMAT)

lib_filterx_tests_test_expr_comparison_CFLAGS  = $(TEST_CFLAGS)
lib_filterx_tests_test_expr_comparison_LDADD   = $(TEST_LDADD) $(JSON_LIBS)

lib_filterx_tests_test_expr_condition_CFLAGS = $(TEST_CFLAGS)
lib_filterx_tests_test_expr_condition_LDADD	 = $(TEST_LDADD) $(JSON_LIBS)
