lib_eventlog_tests_TESTS = \
	lib/eventlog/tests/evtrec \
	lib/eventlog/tests/evtfmt \
	lib/eventlog/tests/evtsyslog \
	lib/eventlog/tests/evtsyslog-macros

EXTRA_DIST += lib/eventlog/tests/CMakeLists.txt

check_PROGRAMS += ${lib_eventlog_tests_TESTS}

lib_eventlog_tests_evtrec_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/lib/eventlog/src
lib_eventlog_tests_evtrec_LDADD = $(top_builddir)/lib/eventlog/src/libevtlog.la

lib_eventlog_tests_evtfmt_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/lib/eventlog/src
lib_eventlog_tests_evtfmt_LDADD = $(top_builddir)/lib/eventlog/src/libevtlog.la

lib_eventlog_tests_evtsyslog_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/lib/eventlog/src
lib_eventlog_tests_evtsyslog_LDADD = $(top_builddir)/lib/eventlog/src/libevtlog.la

lib_eventlog_tests_evtsyslog_macros_SOURCES = lib/eventlog/tests/evtsyslog.c
lib_eventlog_tests_evtsyslog_macros_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/lib/eventlog/src -DEVENTLOG_SYSLOG_MACROS=1
lib_eventlog_tests_evtsyslog_macros_LDADD = $(top_builddir)/lib/eventlog/src/libevtlog.la
