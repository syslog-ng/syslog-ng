modules_graphite_tests_TESTS = \
  modules/graphite/tests/test_graphite_output

check_PROGRAMS       += ${modules_graphite_tests_TESTS}

EXTRA_DIST += modules/graphite/tests/CMakeLists.txt

modules_graphite_tests_test_graphite_output_CFLAGS = $(TEST_CFLAGS)
modules_graphite_tests_test_graphite_output_LDADD = $(TEST_LDADD)
modules_graphite_tests_test_graphite_output_LDFLAGS = \
  $(PREOPEN_SYSLOGFORMAT) \
  -dlpreopen $(top_builddir)/modules/graphite/libgraphite.la
modules_graphite_tests_test_graphite_output_DEPENDENCIES = $(top_builddir)/modules/graphite/libgraphite.la
