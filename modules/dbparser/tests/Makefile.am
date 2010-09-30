AM_CFLAGS = -I$(top_srcdir)/lib -I../../../lib -I$(top_srcdir)/modules/dbparser -I..
LIBS = @DEPS_LIBS@

LDADD = $(top_builddir)/lib/libsyslog-ng.la ../libdbparser.la ../../syslogformat/libsyslogformat.la ../patternize.lo @OPENSSL_LIBS@

check_PROGRAMS = test_timer_wheel test_patternize

test_timer_wheel_SOURCES = test_timer_wheel.c
test_patternize_SOURCES = test_patternize.c

TESTS = $(check_PROGRAMS)
