if ENABLE_JSON
modules_json_tests_TESTS		= \
	modules/json/tests/test_format_json	\
	modules/json/tests/test_jsonparser

check_PROGRAMS				+= ${modules_json_tests_TESTS}

modules_json_tests_test_format_json_CFLAGS	= $(TEST_CFLAGS)
modules_json_tests_test_format_json_LDADD	= $(TEST_LDADD)
modules_json_tests_test_format_json_LDFLAGS	= \
	$(PREOPEN_SYSLOGFORMAT)		  \
	-dlpreopen $(top_builddir)/modules/json/libjson-plugin.la
modules_json_tests_test_format_json_DEPENDENCIES = $(top_builddir)/modules/json/libjson-plugin.la

modules_json_tests_test_jsonparser_CFLAGS	= $(TEST_CFLAGS) -I$(top_srcdir)/modules/json
modules_json_tests_test_jsonparser_LDADD	= $(TEST_LDADD)
modules_json_tests_test_jsonparser_LDFLAGS	= \
	$(PREOPEN_SYSLOGFORMAT)		  \
	-dlpreopen $(top_builddir)/modules/json/libjson-plugin.la
modules_json_tests_test_jsonparser_DEPENDENCIES = $(top_builddir)/modules/json/libjson-plugin.la

endif

