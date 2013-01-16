AM_CFLAGS = -I$(top_srcdir)/lib -I../../../lib -I$(top_srcdir)/libtest -I../../../libtest -I$(top_srcdir)/modules/basicfuncs -I..
AM_LDFLAGS = -dlpreopen ../../syslogformat/libsyslogformat.la -dlpreopen ../libbasicfuncs.la -dlpreopen ../../../lib/libsyslog-ng.la
LDADD = $(top_builddir)/lib/libsyslog-ng.la $(top_builddir)/libtest/libsyslog-ng-test.a @TOOL_DEPS_LIBS@

check_PROGRAMS = test_basicfuncs
TESTS = $(check_PROGRAMS)
