if ENABLE_SNMP_DEST

modules_snmp_dest_tests_TESTS 				= \
  modules/snmp-dest/tests/test_snmp_dest

check_PROGRAMS						+= \
	${modules_snmp_dest_tests_TESTS}

modules_snmp_dest_tests_test_snmp_dest_CFLAGS  = \
	$(TEST_CFLAGS) -I$(top_srcdir)/modules/snmp-dest

modules_snmp_dest_tests_test_snmp_dest_LDADD   = \
	$(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/snmp-dest/libsnmpdest.la

modules_snmp_dest_tests_test_snmp_dest_LDFLAGS = \
	$(PREOPEN_CORE)
	
modules_snmp_dest_tests_test_snmp_dest_DEPENDENCIES = \
	$(top_builddir)/modules/snmp-dest/libsnmpdest.la

endif
EXTRA_DIST += modules/snmp-dest/tests/CMakeLists.txt
