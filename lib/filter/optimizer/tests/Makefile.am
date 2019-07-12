lib_filter_optimizer_tests_TESTS = \
  lib/filter/optimizer/tests/test_optimizer

check_PROGRAMS += ${lib_filter_optimizer_tests_TESTS}

lib_filter_optimizer_tests_test_optimizer_CFLAGS  = $(TEST_CFLAGS) \
  -I${top_srcdir}/lib/filter/optimizer/tests
lib_filter_optimizer_tests_test_optimizer_LDADD   = $(TEST_LDADD) \
  $(PREOPEN_SYSLOGFORMAT)
lib_filter_optimizer_tests_test_optimizer_SOURCES = \
  lib/filter/optimizer/tests/test_optimizer.c

EXTRA_DIST += lib/filter/optimizer/tests/CMakeLists.txt
