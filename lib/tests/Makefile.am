
# we need the dotdot version of the include path to find generated headers
AM_CFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir)/libtest -I../../lib -I../../libtest
LIBS = @TOOL_DEPS_LIBS@
LDADD = $(top_builddir)/lib/libsyslog-ng.la $(top_builddir)/libtest/libsyslog-ng-test.a

check_PROGRAMS =			\
	test_log_message

test_log_message_SOURCES = 			\
	test_log_message.c

TESTS = $(check_PROGRAMS)
