DISKQ_TEST_C_FLAGS = $(TEST_CFLAGS) -I$(top_srcdir)/modules/diskq
DISKQ_TEST_LD_FLAGS = $(TEST_LDFLAGS)
DISKQ_TEST_LD_ADD = $(TEST_LDADD) -dlpreopen $(top_builddir)/modules/diskq/libdisk-buffer.la -lm

modules_diskq_tests_TESTS = \
  modules/diskq/tests/test_diskq \
  modules/diskq/tests/test_diskq_full \
  modules/diskq/tests/test_diskq_truncate \
  modules/diskq/tests/test_reliable_backlog \
  modules/diskq/tests/test_qdisk \
  modules/diskq/tests/test_logqueue_disk \
  modules/diskq/tests/test_diskq_counters

check_PROGRAMS += ${modules_diskq_tests_TESTS}

EXTRA_DIST += modules/diskq/tests/CMakeLists.txt

modules_diskq_tests_test_diskq_CFLAGS = $(DISKQ_TEST_C_FLAGS)
modules_diskq_tests_test_diskq_LDFLAGS = $(DISKQ_TEST_LD_FLAGS)
modules_diskq_tests_test_diskq_LDADD = $(DISKQ_TEST_LD_ADD)
modules_diskq_tests_test_diskq_SOURCES = \
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

modules_diskq_tests_test_diskq_truncate_CFLAGS = $(DISKQ_TEST_C_FLAGS)
modules_diskq_tests_test_diskq_truncate_LDFLAGS = $(DISKQ_TEST_LD_FLAGS)
modules_diskq_tests_test_diskq_truncate_LDADD = $(DISKQ_TEST_LD_ADD)
modules_diskq_tests_test_diskq_truncate_SOURCES = \
	modules/diskq/tests/test_diskq_truncate.c \
	modules/diskq/tests/test_diskq_tools.h

modules_diskq_tests_test_qdisk_CFLAGS = $(DISKQ_TEST_C_FLAGS)
modules_diskq_tests_test_qdisk_LDFLAGS = $(DISKQ_TEST_LD_FLAGS)
modules_diskq_tests_test_qdisk_LDADD = $(DISKQ_TEST_LD_ADD)
modules_diskq_tests_test_qdisk_SOURCES = \
	modules/diskq/tests/test_qdisk.c \
	modules/diskq/tests/test_diskq_tools.h

modules_diskq_tests_test_logqueue_disk_CFLAGS = $(DISKQ_TEST_C_FLAGS)
modules_diskq_tests_test_logqueue_disk_LDFLAGS = $(DISKQ_TEST_LD_FLAGS)
modules_diskq_tests_test_logqueue_disk_LDADD = $(DISKQ_TEST_LD_ADD)
modules_diskq_tests_test_logqueue_disk_SOURCES = \
	modules/diskq/tests/test_logqueue_disk.c \
	modules/diskq/tests/test_diskq_tools.h

modules_diskq_tests_test_diskq_counters_CFLAGS = $(DISKQ_TEST_C_FLAGS)
modules_diskq_tests_test_diskq_counters_LDFLAGS = $(DISKQ_TEST_LD_FLAGS)
modules_diskq_tests_test_diskq_counters_LDADD = $(DISKQ_TEST_LD_ADD)
modules_diskq_tests_test_diskq_counters_SOURCES = \
	modules/diskq/tests/test_diskq_counters.c \
	modules/diskq/tests/test_diskq_tools.h
