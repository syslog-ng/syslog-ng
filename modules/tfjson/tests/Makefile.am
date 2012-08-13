AM_CFLAGS = -I$(top_srcdir)/lib -I../../../lib -I$(top_srcdir)/libtest -I../../../libtest -I$(top_srcdir)/modules/tfjson -I..
AM_LDFLAGS = -dlpreopen ../../syslogformat/libsyslogformat.la -dlpreopen ../libtfjson.la
LDADD = $(top_builddir)/lib/libsyslog-ng.la $(top_builddir)/libtest/libsyslog-ng-test.a @TOOL_DEPS_LIBS@

check_PROGRAMS = test_json
TESTS = $(check_PROGRAMS)
