lib_debugger_tests_TESTS		 = \
	lib/debugger/tests/test_debugger

EXTRA_DIST += lib/debugger/tests/CMakeLists.txt

check_PROGRAMS				+= ${lib_debugger_tests_TESTS}

lib_debugger_tests_test_debugger_CFLAGS  = $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/debugger/tests
lib_debugger_tests_test_debugger_LDADD	 = $(TEST_LDADD)
lib_debugger_tests_test_debugger_SOURCES = 			\
	lib/debugger/tests/test-debugger.c
