AM_CFLAGS = -I$(top_srcdir)/lib -I../../../lib -I$(top_srcdir)/modules/csvparser -I..
AM_LDFLAGS = -dlpreopen ../../syslogformat/libsyslogformat.la -dlpreopen ../libcsvparser.la
LDADD = $(top_builddir)/lib/libsyslog-ng.la $(top_builddir)/libtest/libsyslog-ng-test.a @TOOL_DEPS_LIBS@

check_PROGRAMS = test_csvparser
TESTS = $(check_PROGRAMS)
