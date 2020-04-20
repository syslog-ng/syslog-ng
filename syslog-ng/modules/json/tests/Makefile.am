if ENABLE_JSON
modules_json_tests_TESTS		= \
	modules/json/tests/test_format_json	\
	modules/json/tests/test_json_parser	\
	modules/json/tests/test_dot_notation

check_PROGRAMS				+= ${modules_json_tests_TESTS}

modules_json_tests_test_format_json_CFLAGS	= $(TEST_CFLAGS)
modules_json_tests_test_format_json_LDADD	= $(TEST_LDADD)
modules_json_tests_test_format_json_LDFLAGS	= \
	$(PREOPEN_SYSLOGFORMAT)		  \
	-dlpreopen $(top_builddir)/modules/json/libjson-plugin.la
modules_json_tests_test_format_json_DEPENDENCIES = $(top_builddir)/modules/json/libjson-plugin.la

modules_json_tests_test_json_parser_CFLAGS	= $(TEST_CFLAGS) -I$(top_srcdir)/modules/json
modules_json_tests_test_json_parser_LDADD	= $(TEST_LDADD)
modules_json_tests_test_json_parser_LDFLAGS	= \
	$(PREOPEN_SYSLOGFORMAT)		  \
	-dlpreopen $(top_builddir)/modules/json/libjson-plugin.la
modules_json_tests_test_json_parser_DEPENDENCIES = $(top_builddir)/modules/json/libjson-plugin.la

modules_json_tests_test_dot_notation_CFLAGS	= $(TEST_CFLAGS) $(JSON_CFLAGS) -I$(top_srcdir)/modules/json
modules_json_tests_test_dot_notation_LDADD	= $(TEST_LDADD) $(JSON_LIBS)
modules_json_tests_test_dot_notation_LDFLAGS	= \
	$(PREOPEN_SYSLOGFORMAT)		  \
	-dlpreopen $(top_builddir)/modules/json/libjson-plugin.la
modules_json_tests_test_dot_notation_DEPENDENCIES = $(top_builddir)/modules/json/libjson-plugin.la

endif

EXTRA_DIST += modules/json/tests/CMakeLists.txt
