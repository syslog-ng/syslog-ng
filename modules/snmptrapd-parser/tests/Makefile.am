modules_snmptrapd_parser_tests_TESTS                             = \
  modules/snmptrapd-parser/tests/test_varbindlist_scanner          \
  modules/snmptrapd-parser/tests/test_snmptrapd_parser

check_PROGRAMS                                                   += ${modules_snmptrapd_parser_tests_TESTS}

modules_snmptrapd_parser_tests_test_varbindlist_scanner_CFLAGS   = \
  $(TEST_CFLAGS) -I$(top_srcdir)/modules/snmptrapd-parser
modules_snmptrapd_parser_tests_test_varbindlist_scanner_LDADD    = $(TEST_LDADD) \
  -dlpreopen $(top_builddir)/modules/snmptrapd-parser/libsnmptrapd-parser.la

modules_snmptrapd_parser_tests_test_snmptrapd_parser_CFLAGS   = \
  $(TEST_CFLAGS) -I$(top_srcdir)/modules/snmptrapd-parser
modules_snmptrapd_parser_tests_test_snmptrapd_parser_LDADD    = $(TEST_LDADD) \
  -dlpreopen $(top_builddir)/modules/snmptrapd-parser/libsnmptrapd-parser.la
