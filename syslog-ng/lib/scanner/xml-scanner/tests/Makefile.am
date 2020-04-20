lib_xml_scanner_tests_TESTS		= \
	lib/scanner/xml-scanner/tests/test_xml_scanner

EXTRA_DIST += lib/scanner/xml-scanner/tests/CMakeLists.txt

check_PROGRAMS		+= ${lib_xml_scanner_tests_TESTS}

lib_scanner_xml_scanner_tests_test_xml_scanner_CFLAGS	= $(TEST_CFLAGS) -I$(top_srcdir)/lib/scanner/xml-scanner
lib_scanner_xml_scanner_tests_test_xml_scanner_LDADD	=	$(TEST_LDADD)
