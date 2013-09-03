export top_srcdir

TEST_CFLAGS = -I$(top_srcdir)/libtest -I$(top_srcdir)/lib
TEST_LDADD = $(top_builddir)/lib/libsyslog-ng.la $(top_builddir)/libtest/libsyslog-ng-test.a @TOOL_DEPS_LIBS@

template_tests_TESTS = template/tests/test_template_compile
check_PROGRAMS	= ${template_tests_TESTS}

template_tests_test_template_compile_CFLAGS	=	$(TEST_CFLAGS)
template_tests_test_template_compile_LDADD	=	$(TEST_LDADD)
