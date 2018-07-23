REDISQ_TEST_C_FLAGS = $(TEST_CFLAGS) -I$(top_srcdir)/modules/redisq @CFLAGS_NOWARN_POINTER_SIGN@
REDISQ_TEST_LD_FLAGS = $(TEST_LDFLAGS) ${PREOPEN_SYSLOGFORMAT} -dlpreopen $(top_builddir)/modules/redisq/libredis-queue.la
REDISQ_TEST_LD_ADD = $(TEST_LDADD)

modules_redisq_tests_TESTS = \
  modules/redisq/tests/test_redisq

check_PROGRAMS += ${modules_redisq_tests_TESTS}

EXTRA_DIST += modules/redisq/tests/CMakeLists.txt

modules_redisq_tests_test_redisq_CFLAGS = $(REDISQ_TEST_C_FLAGS)
modules_redisq_tests_test_redisq_LDFLAGS = $(REDISQ_TEST_LD_FLAGS)
modules_redisq_tests_test_redisq_LDADD = $(REDISQ_TEST_LD_ADD)
modules_redisq_tests_test_redisq_SOURCE = \
	modules/redisq/tests/test_redisq.c
