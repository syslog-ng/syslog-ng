modules_http_tests_TESTS			= \
	modules/http/tests/test_http

EXTRA_DIST += modules/http/tests/CMakeLists.txt

check_PROGRAMS					+= ${modules_http_tests_TESTS}

modules_http_tests_test_http_DEPENDENCIES =      \
        $(top_builddir)/modules/http/libhttp.la
modules_http_tests_test_http_CFLAGS	= $(TEST_CFLAGS) -I$(top_srcdir)/modules/http
modules_http_tests_test_http_LDADD	= \
	$(TEST_LDADD)				  \
	$(PREOPEN_SYSLOGFORMAT) -dlpreopen $(top_builddir)/modules/http/libhttp.la
