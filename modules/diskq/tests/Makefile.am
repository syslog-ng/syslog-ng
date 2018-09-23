DISKQ_TEST_C_FLAGS = $(TEST_CFLAGS) -I$(top_srcdir)/modules/diskq
DISKQ_TEST_LD_FLAGS = $(TEST_LDFLAGS) ${PREOPEN_SYSLOGFORMAT} -dlpreopen $(top_builddir)/modules/diskq/libdisk-buffer.la
DISKQ_TEST_LD_ADD = $(TEST_LDADD)

modules_diskq_tests_TESTS = \
  modules/diskq/tests/test_diskq \
  modules/diskq/tests/test_diskq_full \
  modules/diskq/tests/test_reliable_backlog

check_PROGRAMS += ${modules_diskq_tests_TESTS}

EXTRA_DIST += modules/diskq/tests/CMakeLists.txt

modules_diskq_tests_test_diskq_CFLAGS = $(DISKQ_TEST_C_FLAGS)
modules_diskq_tests_test_diskq_LDFLAGS = $(DISKQ_TEST_LD_FLAGS)
modules_diskq_tests_test_diskq_LDADD = $(DISKQ_TEST_LD_ADD)
modules_diskq_tests_test_diskq_SOURCE = \
	modules/diskq/tests/test_diskq.c \
	modules/diskq/tests/test_diskq_tools.h

modules_diskq_tests_test_diskq_full_CFLAGS = $(DISKQ_TEST_C_FLAGS)
modules_diskq_tests_test_diskq_full_LDFLAGS = $(DISKQ_TEST_LD_FLAGS)
modules_diskq_tests_test_diskq_full_LDADD = $(DISKQ_TEST_LD_ADD)
modules_diskq_tests_test_diskq_full_SOURCES = \
	modules/diskq/tests/test_diskq_full.c \
	modules/diskq/tests/test_diskq_tools.h

modules_diskq_tests_test_reliable_backlog_CFLAGS = $(DISKQ_TEST_C_FLAGS)
modules_diskq_tests_test_reliable_backlog_LDFLAGS = $(DISKQ_TEST_LD_FLAGS)
modules_diskq_tests_test_reliable_backlog_LDADD = $(DISKQ_TEST_LD_ADD)
modules_diskq_tests_test_reliable_backlog_SOURCES = \
	modules/diskq/tests/test_reliable_backlog.c \
	modules/diskq/tests/test_diskq_tools.h
