lib_kv_scanner_tests_TESTS		= \
	lib/scanner/kv-scanner/tests/test_kv_scanner

EXTRA_DIST += lib/scanner/kv-scanner/tests/CMakeLists.txt

check_PROGRAMS		+= ${lib_kv_scanner_tests_TESTS}

lib_scanner_kv_scanner_tests_test_kv_scanner_CFLAGS	= $(TEST_CFLAGS) -I$(top_srcdir)/lib/scanner/kv-scanner
lib_scanner_kv_scanner_tests_test_kv_scanner_LDADD	=	$(TEST_LDADD)
