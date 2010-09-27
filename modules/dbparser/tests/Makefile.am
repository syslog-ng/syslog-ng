AM_CFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir)/modules/dbparser -I..
LIBS = @DEPS_LIBS@

LDADD = $(top_builddir)/lib/libsyslog-ng.la ../libdbparser.la

check_PROGRAMS = test_timer_wheel

test_timer_wheel_SOURCES = test_timer_wheel.c

TESTS = $(check_PROGRAMS)
