modules_kvformat_tests_TESTS		= \
	modules/kvformat/tests/test_format_welf	\
	modules/kvformat/tests/test_linux_audit_scanner	\
	modules/kvformat/tests/test_kv_parser

check_PROGRAMS				+= ${modules_kvformat_tests_TESTS}

EXTRA_DIST += modules/kvformat/tests/CMakeLists.txt

modules_kvformat_tests_test_format_welf_CFLAGS	= $(TEST_CFLAGS)
modules_kvformat_tests_test_format_welf_LDADD	= $(TEST_LDADD)
modules_kvformat_tests_test_format_welf_LDFLAGS	= \
	$(PREOPEN_SYSLOGFORMAT)		  \
	-dlpreopen $(top_builddir)/modules/kvformat/libkvformat.la
modules_kvformat_tests_test_format_welf_DEPENDENCIES = $(top_builddir)/modules/kvformat/libkvformat.la

modules_kvformat_tests_test_kv_parser_CFLAGS	= $(TEST_CFLAGS) -I$(top_srcdir)/modules/kvformat
modules_kvformat_tests_test_kv_parser_LDADD	= $(TEST_LDADD)
modules_kvformat_tests_test_kv_parser_LDFLAGS	= \
	$(PREOPEN_SYSLOGFORMAT)		  \
	-dlpreopen $(top_builddir)/modules/kvformat/libkvformat.la
modules_kvformat_tests_test_kv_parser_DEPENDENCIES = $(top_builddir)/modules/kvformat/libkvformat.la

modules_kvformat_tests_test_linux_audit_scanner_CFLAGS	= $(TEST_CFLAGS) -I$(top_srcdir)/modules/kvformat
modules_kvformat_tests_test_linux_audit_scanner_LDADD	= $(TEST_LDADD)
modules_kvformat_tests_test_linux_audit_scanner_LDFLAGS	= \
	-dlpreopen $(top_builddir)/modules/kvformat/libkvformat.la
modules_kvformat_tests_test_linux_audit_scanner_DEPENDENCIES = $(top_builddir)/modules/kvformat/libkvformat.la
