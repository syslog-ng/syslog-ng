AM_CFLAGS = -I$(top_srcdir)/lib -I../../../lib -I$(top_srcdir)/modules/dbparser -I.. @CFLAGS_NOWARN_POINTER_SIGN@
AM_LDFLAGS = -dlpreopen ../../syslogformat/libsyslogformat.la
LDADD = ../libsyslog-ng-patterndb.a $(top_builddir)/lib/libsyslog-ng.la $(top_builddir)/lib/libsyslog-ng-crypto.la @TOOL_DEPS_LIBS@

check_PROGRAMS = test_timer_wheel test_patternize test_patterndb test_radix

test_timer_wheel_SOURCES = test_timer_wheel.c
test_patternize_SOURCES = test_patternize.c
test_patterndb_SOURCES = test_patterndb.c

test_radix_SOURCES = test_radix.c

TESTS = $(check_PROGRAMS)
