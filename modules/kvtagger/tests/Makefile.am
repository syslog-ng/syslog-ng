modules_kvtagger_tests_TESTS                   =       \
        modules/kvtagger/tests/test_kvtagdb

check_PROGRAMS                                  +=      \
        ${modules_kvtagger_tests_TESTS}

modules_kvtagger_tests_test_kvtagdb_CFLAGS   =       \
        $(TEST_CFLAGS) -I$(top_srcdir)/modules/kvtagger
modules_kvtagger_tests_test_kvtagdb_LDADD    =       \
        $(TEST_LDADD)
modules_kvtagger_tests_test_kvtagdb_LDFLAGS  =       \
        $(PREOPEN_SYSLOGFORMAT)                         \
        -dlpreopen $(top_builddir)/modules/kvtagger/libkvtagger.la
