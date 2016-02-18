DISKQ_TEST_CFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir)/libtest -I$(top_srcdir)/modules/diskq @CFLAGS_NOWARN_POINTER_SIGN@
DISKQ_TEST_LDFLAGS = ${PREOPEN_SYSLOGFORMAT}
DISKQ_TEST_LDADD = $(top_builddir)/libtest/libsyslog-ng-test.a $(top_builddir)/modules/diskq/libsyslog-ng-disk-buffer.la $(MODULE_DEPS_LIBS) @TOOL_DEPS_LIBS@ @OPENSSL_LIBS@

modules_diskq_tests_TESTS = \
  modules/diskq/tests/test_diskq \
  modules/diskq/tests/test_diskq_full \
  modules/diskq/tests/test_reliable_backlog

check_PROGRAMS += ${modules_diskq_tests_TESTS}

modules_diskq_tests_test_diskq_CFLAGS = $(TEST_CFLAGS) $(DISKQ_TEST_CFLAGS)
modules_diskq_tests_test_diskq_LDFLAGS = $(TEST_LDFLAGS) $(DISKQ_TEST_LDFLAGS)
modules_diskq_tests_test_diskq_LDADD = $(TEST_LDADD) $(DISKQ_TEST_LDADD)
modules_diskq_tests_test_diskq_SOURCE = modules/diskq/tests/test_diskq.c modules/diskq/tests/test_diskq_tools.h

modules_diskq_tests_test_diskq_full_CFLAGS = $(TEST_CFLAGS) $(DISKQ_TEST_CFLAGS)
modules_diskq_tests_test_diskq_full_LDFLAGS = $(TEST_LDLAGS) $(DISKQ_TEST_LDFLAGS)
modules_diskq_tests_test_diskq_full_LDADD = $(TEST_LDADD) $(DISKQ_TEST_LDADD)
modules_diskq_tests_test_diskq_full_SOURCES =  modules/diskq/tests/test_diskq_full.c modules/diskq/tests/test_diskq_tools.h

modules_diskq_tests_test_reliable_backlog_CFLAGS = $(TEST_CFLAGS) $(DISKQ_TEST_CFLAGS)
modules_diskq_tests_test_reliable_backlog_LDFLAGS = $(TEST_LDLAGS) $(DISKQ_TEST_LDFLAGS)
modules_diskq_tests_test_reliable_backlog_LDADD = $(TEST_LDADD) $(DISKQ_TEST_LDADD)
modules_diskq_tests_test_reliable_backlog_SOURCES =  modules/diskq/tests/test_reliable_backlog.c  modules/diskq/tests/test_diskq_tools.h

