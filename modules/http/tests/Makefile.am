if ENABLE_HTTP

modules_http_tests_TESTS			= \
	modules/http/tests/test_http			\
	modules/http/tests/test_http-loadbalancer

check_PROGRAMS					+= ${modules_http_tests_TESTS}

modules_http_tests_test_http_DEPENDENCIES =      \
        $(top_builddir)/modules/http/libhttp.la
modules_http_tests_test_http_CFLAGS	= $(TEST_CFLAGS) -I$(top_srcdir)/modules/http
modules_http_tests_test_http_LDADD	= $(TEST_LDADD)
modules_http_tests_test_http_LDFLAGS	= \
	-dlpreopen $(top_builddir)/modules/http/libhttp.la


modules_http_tests_test_http_loadbalancer_DEPENDENCIES = \
	$(top_builddir)/modules/http/libhttp.la
modules_http_tests_test_http_loadbalancer_CFLAGS	= $(TEST_CFLAGS)
modules_http_tests_test_http_loadbalancer_LDADD		= $(TEST_LDADD)
modules_http_tests_test_http_loadbalancer_LDFLAGS	= \
	-dlpreopen $(top_builddir)/modules/http/libhttp.la

endif

EXTRA_DIST += modules/http/tests/CMakeLists.txt
