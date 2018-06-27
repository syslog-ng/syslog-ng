modules_xml_tests_TESTS		= \
	modules/xml/tests/test_xml_parser

check_PROGRAMS				+= ${modules_xml_tests_TESTS}

modules_xml_tests_test_xml_parser_CFLAGS	= $(TEST_CFLAGS) -I$(top_srcdir)/modules/xml
modules_xml_tests_test_xml_parser_LDADD	= $(TEST_LDADD)
modules_xml_tests_test_xml_parser_LDFLAGS	= \
	$(PREOPEN_SYSLOGFORMAT)		  \
	-dlpreopen $(top_builddir)/modules/xml/libxml.la
modules_xml_tests_test_xml_parser_DEPENDENCIES = $(top_builddir)/modules/xml/libxml.la

EXTRA_DIST += modules/xml/tests/CMakeLists.txt
