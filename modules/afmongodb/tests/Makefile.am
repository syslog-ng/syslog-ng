modules_afmongodb_tests_TESTS          = \
       modules/afmongodb/tests/test-mongodb-config

check_PROGRAMS                         += ${modules_afmongodb_tests_TESTS}

modules_afmongodb_tests_test_mongodb_config_CFLAGS = \
    $(LIBMONGO_CFLAGS) \
    $(TEST_CFLAGS)

modules_afmongodb_tests_test_mongodb_config_LDADD        = \
    $(TEST_LDADD) \
    -dlpreopen $(top_builddir)/modules/afmongodb/libafmongodb.la \
    ${lmc_EXTRA_DEPS}
