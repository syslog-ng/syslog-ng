DISKQ_TEST_C_FLAGS = -I$(top_srcdir)/lib -I$(top_srcdir)/libtest -I$(top_srcdir)/modules/diskq @CFLAGS_NOWARN_POINTER_SIGN@
DISKQ_TEST_LD_FLAGS = ${PREOPEN_SYSLOGFORMAT}
DISKQ_TEST_LD_ADD = $(top_builddir)/libtest/libsyslog-ng-test.a $(LIBSYSLOG_NG_DISK_BUFFER) $(MODULE_DEPS_LIBS) @TOOL_DEPS_LIBS@ @OPENSSL_LIBS@

modules_diskq_tests_TESTS = \
  modules/diskq/tests/test_diskq \
  modules/diskq/tests/test_diskq_full \
  modules/diskq/tests/test_reliable_backlog

check_PROGRAMS += ${modules_diskq_tests_TESTS}

EXTRA_DIST += modules/diskq/tests/CMakeLists.txt

modules_diskq_tests_test_diskq_CFLAGS = $(TEST_CFLAGS) $(DISKQ_TEST_C_FLAGS)
modules_diskq_tests_test_diskq_LDFLAGS = $(TEST_LDFLAGS) $(DISKQ_TEST_LD_FLAGS)
modules_diskq_tests_test_diskq_LDADD = $(TEST_LDADD) $(DISKQ_TEST_LD_ADD)
modules_diskq_tests_test_diskq_SOURCE = modules/diskq/tests/test_diskq.c modules/diskq/tests/test_diskq_tools.h

modules_diskq_tests_test_diskq_full_CFLAGS = $(TEST_CFLAGS) $(DISKQ_TEST_C_FLAGS)
modules_diskq_tests_test_diskq_full_LDFLAGS = $(TEST_LDLAGS) $(DISKQ_TEST_LD_FLAGS)
modules_diskq_tests_test_diskq_full_LDADD = $(TEST_LDADD) $(DISKQ_TEST_LD_ADD)
modules_diskq_tests_test_diskq_full_SOURCES =  modules/diskq/tests/test_diskq_full.c modules/diskq/tests/test_diskq_tools.h

modules_diskq_tests_test_reliable_backlog_CFLAGS = $(TEST_CFLAGS) $(DISKQ_TEST_C_FLAGS)
modules_diskq_tests_test_reliable_backlog_LDFLAGS = $(TEST_LDLAGS) $(DISKQ_TEST_LD_FLAGS)
modules_diskq_tests_test_reliable_backlog_LDADD = $(TEST_LDADD) $(DISKQ_TEST_LD_ADD)
modules_diskq_tests_test_reliable_backlog_SOURCES =  modules/diskq/tests/test_reliable_backlog.c  modules/diskq/tests/test_diskq_tools.h
