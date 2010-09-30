AM_CFLAGS = -I$(top_srcdir)/lib -I../../../lib -I$(top_srcdir)/modules/dbparser -I..
LIBS = @DEPS_LIBS@

LDADD = $(top_builddir)/lib/libsyslog-ng.la ../libdbparser.la ../../syslogformat/libsyslogformat.la ../patternize.lo @OPENSSL_LIBS@

check_PROGRAMS = test_timer_wheel test_patternize test_patterndb_v2 test_patterndb_v3 test_radix

test_timer_wheel_SOURCES = test_timer_wheel.c
test_patternize_SOURCES = test_patternize.c

test_patterndb_v2_SOURCES = test_patterndb_v2.c
test_patterndb_v3_SOURCES = test_patterndb_v3.c
test_radix_SOURCES = test_radix.c

TESTS = $(check_PROGRAMS)
