lib_csv_scanner_tests_TESTS		= \
	lib/scanner/csv-scanner/tests/test_csv_scanner

EXTRA_DIST += lib/scanner/csv-scanner/tests/CMakeLists.txt

check_PROGRAMS		+= ${lib_csv_scanner_tests_TESTS}

lib_scanner_csv_scanner_tests_test_csv_scanner_CFLAGS	= $(TEST_CFLAGS) -I$(top_srcdir)/lib/scanner/csv-scanner
lib_scanner_csv_scanner_tests_test_csv_scanner_LDADD	=	$(TEST_LDADD)
