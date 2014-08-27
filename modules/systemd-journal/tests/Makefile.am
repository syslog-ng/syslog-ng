moduledir = @moduledir@
export top_srcdir

AM_CPPFLAGS = -I$(top_srcdir)/libtest -I$(top_srcdir)/lib -I../../lib -I$(top_srcdir)/modules/systemd-journal

AM_LDFLAGS = $(top_builddir)/modules/systemd-journal/libsdjournal.la

LDADD = $(top_builddir)/lib/libsyslog-ng.la $(top_builddir)/libtest/libsyslog-ng-test.a @TOOL_DEPS_LIBS@

check_PROGRAMS = test_systemd_journal

TESTS = $(check_PROGRAMS)

test_systemd_journal_SOURCES = \
	test_systemd_journal.c \
	journald-mock.c \
	journald-mock.h \
	test-source.h \
	test-source.c
