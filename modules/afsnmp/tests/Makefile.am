if ENABLE_AFSNMP

modules_afsnmp_tests_TESTS 				= \
  modules/afsnmp/tests/test_afsnmp_dest \
  modules/afsnmp/tests/test_varbindlist_scanner \
  modules/afsnmp/tests/test_snmptrapd_parser


check_PROGRAMS						+= \
	${modules_afsnmp_tests_TESTS}


modules_afsnmp_tests_test_afsnmp_dest_CFLAGS  = \
	$(TEST_CFLAGS) -I$(top_srcdir)/modules/afsnmp

modules_afsnmp_tests_test_afsnmp_dest_LDADD   = \
	$(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/afsnmp/libafsnmp.la

modules_afsnmp_tests_test_afsnmp_dest_LDFLAGS = \
	$(PREOPEN_CORE)
	
modules_afsnmp_tests_test_afsnmp_dest_DEPENDENCIES = \
	$(top_builddir)/modules/afsnmp/libafsnmp.la

modules_afsnmp_tests_test_varbindlist_scanner_CFLAGS   = \
  $(TEST_CFLAGS) -I$(top_srcdir)/modules/afsnmp
modules_afsnmp_tests_test_varbindlist_scanner_LDADD    = $(TEST_LDADD) \
  -dlpreopen $(top_builddir)/modules/afsnmp/libafsnmp.la

modules_afsnmp_tests_test_snmptrapd_parser_CFLAGS   = \
  $(TEST_CFLAGS) -I$(top_srcdir)/modules/afsnmp
modules_afsnmp_tests_test_snmptrapd_parser_LDADD    = $(TEST_LDADD) \
  -dlpreopen $(top_builddir)/modules/afsnmp/libafsnmp.la

endif
EXTRA_DIST += modules/afsnmp/tests/CMakeLists.txt
