AM_CFLAGS = -I$(top_srcdir)/lib -I../../../lib -I$(top_srcdir)/libtest -I../../../libtest -I$(top_srcdir)/modules/json -I..
AM_LDFLAGS = -dlpreopen ../../syslogformat/libsyslogformat.la -dlpreopen ../libjson-plugin.la
LDADD = $(top_builddir)/lib/libsyslog-ng.la $(top_builddir)/libtest/libsyslog-ng-test.a @TOOL_DEPS_LIBS@


if ENABLE_JSON

check_PROGRAMS = test_json
TESTS = $(check_PROGRAMS)

endif
