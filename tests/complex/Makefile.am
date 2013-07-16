AM_CFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir)/libtest -I../../lib
AM_LDFLAGS = -dlpreopen $(top_builddir)/modules/syslogformat/libsyslogformat.la  $(top_builddir)/modules/basic-proto/libbasic-proto.la
LIBS = @TOOL_DEPS_LIBS@
LDADD = $(top_builddir)/lib/libsyslog-ng.la $(top_builddir)/libtest/libsyslog-ng-test.a @TOOL_DEPS_LIBS@

check_PROGRAMS = test_control

test_control_SOURCES = test_control.c
test_control_CFLAGS = -I$(top_srcdir)/syslog-ng-ctl -I$(top_srcdir)/lib -I$(top_srcdir)/libtest -I../../lib

TESTS = $(check_PROGRAMS)
