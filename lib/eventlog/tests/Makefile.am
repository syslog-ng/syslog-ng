lib_eventlog_tests_TESTS = \
	lib/eventlog/tests/test_evtrec \
	lib/eventlog/tests/test_evtfmt \
	lib/eventlog/tests/test_evtsyslog \
	lib/eventlog/tests/test_evtsyslog_macros \
	lib/eventlog/tests/test_evt_tag_mem

EXTRA_DIST += lib/eventlog/tests/CMakeLists.txt

check_PROGRAMS += ${lib_eventlog_tests_TESTS}

lib_eventlog_tests_test_evtrec_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/lib/eventlog/src
lib_eventlog_tests_test_evtrec_LDADD = $(top_builddir)/lib/eventlog/src/libevtlog.la

lib_eventlog_tests_test_evtfmt_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/lib/eventlog/src
lib_eventlog_tests_test_evtfmt_LDADD = $(top_builddir)/lib/eventlog/src/libevtlog.la

lib_eventlog_tests_test_evtsyslog_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/lib/eventlog/src
lib_eventlog_tests_test_evtsyslog_LDADD = $(top_builddir)/lib/eventlog/src/libevtlog.la

lib_eventlog_tests_test_evt_tag_mem_CFLAGS = $(TEST_CFLAGS)
lib_eventlog_tests_test_evt_tag_mem_LDADD = $(TEST_LDADD)

lib_eventlog_tests_test_evtsyslog_macros_SOURCES = lib/eventlog/tests/test_evtsyslog.c
lib_eventlog_tests_test_evtsyslog_macros_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/lib/eventlog/src -DEVENTLOG_SYSLOG_MACROS=1
lib_eventlog_tests_test_evtsyslog_macros_LDADD = $(top_builddir)/lib/eventlog/src/libevtlog.la
