check_PROGRAMS						+= \
	${modules_affile_tests_TESTS}

modules_affile_tests_TESTS 				= \
  modules/affile/tests/test_wildcard_source \
	modules/affile/tests/test_directory_monitor \
	modules/affile/tests/test_collection_comporator \
	modules/affile/tests/test_file_opener \
	modules/affile/tests/test_file_state_handler \
	modules/affile/tests/test_file_list

modules_affile_tests_test_wildcard_source_CFLAGS  = $(TEST_CFLAGS) -I$(top_srcdir)/modules/affile
modules_affile_tests_test_wildcard_source_LDADD   = $(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/affile/libaffile.la

modules_affile_tests_test_directory_monitor_CFLAGS  = $(TEST_CFLAGS) -I$(top_srcdir)/modules/affile
modules_affile_tests_test_directory_monitor_LDADD   = $(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/affile/libaffile.la

modules_affile_tests_test_collection_comporator_CFLAGS  = $(TEST_CFLAGS) -I$(top_srcdir)/modules/affile
modules_affile_tests_test_collection_comporator_LDADD   = $(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/affile/libaffile.la

modules_affile_tests_test_file_opener_CFLAGS 	= $(TEST_CFLAGS) -I$(top_srcdir)/modules/affile
modules_affile_tests_test_file_opener_LDADD	= $(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/affile/libaffile.la

modules_affile_tests_test_file_state_handler_CFLAGS = $(TEST_CFLAGS) -I$(top_srcdir)/modules/affile
modules_affile_tests_test_file_state_handler_LDADD	= $(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/affile/libaffile.la

modules_affile_tests_test_file_list_CFLAGS = $(TEST_CFLAGS) -I$(top_srcdir)/modules/affile
modules_affile_tests_test_file_list_LDADD	= $(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/affile/libaffile.la