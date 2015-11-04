modules_snmp_format_tests_TESTS		= \
	modules/snmp-format/tests/test_snmp_format

check_PROGRAMS				+= ${modules_snmp_format_tests_TESTS}

modules_snmp_format_tests_test_snmp_format_CFLAGS	= \
	$(TEST_CFLAGS) -I$(top_srcdir)/modules/snmp-format
modules_snmp_format_tests_test_snmp_format_LDADD	= $(TEST_LDADD)
modules_snmp_format_tests_test_snmp_format_LDFLAGS	= \
	$(PREOPEN_SYSLOGFORMAT)		  \
	-dlpreopen $(top_builddir)/modules/snmp-format/libsnmp-format.la
modules_snmp_format_tests_test_snmp_format_DEPENDENCIES = \
	$(top_builddir)/modules/snmp-format/libsnmp-format.la
