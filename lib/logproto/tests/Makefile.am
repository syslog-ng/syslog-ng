
# we need the dotdot version of the include path to find generated headers
AM_CFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir)/libtest -I../../../lib -I../../../libtest
LIBS = @TOOL_DEPS_LIBS@
LDADD = $(top_builddir)/lib/libsyslog-ng.la $(top_builddir)/libtest/libsyslog-ng-test.a

check_PROGRAMS =			\
	test_logproto			\
	test_findeom

test_logproto_SOURCES = 			\
	test_logproto.c 			\
	test-server-options.c			\
	test-record-server.c			\
	test-text-server.c			\
	test-dgram-server.c			\
	test-framed-server.c

test_findeom_SOURCES = test_findeom.c

TESTS = $(check_PROGRAMS)
