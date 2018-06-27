modules_cef_tests_TESTS		= \
	modules/cef/tests/test-format-cef-extension

check_PROGRAMS				+= ${modules_cef_tests_TESTS}

EXTRA_DIST += modules/cef/tests/CMakeLists.txt

modules_cef_tests_test_format_cef_extension_CFLAGS	= $(TEST_CFLAGS)
modules_cef_tests_test_format_cef_extension_LDADD	= $(TEST_LDADD)
modules_cef_tests_test_format_cef_extension_LDFLAGS	= \
	$(PREOPEN_SYSLOGFORMAT)		  \
	-dlpreopen $(top_builddir)/modules/cef/libcef.la

modules_cef_tests_test_format_cef_extension_DEPENDENCIES =	\
	$(top_builddir)/modules/cef/libcef.la
