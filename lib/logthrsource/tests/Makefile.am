lib_logthrsource_tests_TESTS = \
  lib/logthrsource/tests/test_logthrsourcedrv \
  lib/logthrsource/tests/test_logthrfetcherdrv

EXTRA_DIST += lib/logthrsource/tests/CMakeLists.txt

check_PROGRAMS += ${lib_logthrsource_tests_TESTS}

lib_logthrsource_tests_test_logthrsourcedrv_CFLAGS = $(TEST_CFLAGS)
lib_logthrsource_tests_test_logthrsourcedrv_LDADD = $(TEST_LDADD)

lib_logthrsource_tests_test_logthrfetcherdrv_CFLAGS = $(TEST_CFLAGS)
lib_logthrsource_tests_test_logthrfetcherdrv_LDADD = $(TEST_LDADD)
