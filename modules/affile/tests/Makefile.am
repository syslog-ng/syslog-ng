modules_affile_tests_TESTS				= \
	modules/affile/tests/test_affile_open_file \
	modules/affile/tests/test_wildcard_source \
	modules/affile/tests/test_directory_monitor \
	modules/affile/tests/test_collection_comporator

check_PROGRAMS						+= \
	${modules_affile_tests_TESTS}

modules_affile_tests_test_affile_open_file_CFLAGS 	= $(TEST_CFLAGS)
modules_affile_tests_test_affile_open_file_LDADD	= $(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/affile/libaffile.la
modules_affile_tests_test_affile_open_file_LDFLAGS 	=   \
	$(PREOPEN_CORE)

modules_affile_tests_test_wildcard_source_CFLAGS  = $(TEST_CFLAGS) -I$(top_srcdir)/modules/affile
modules_affile_tests_test_wildcard_source_LDADD   = $(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/affile/libaffile.la
modules_affile_tests_test_wildcard_source_LDFLAGS = $(PREOPEN_CORE)

modules_affile_tests_test_directory_monitor_CFLAGS  = $(TEST_CFLAGS) -I$(top_srcdir)/modules/affile
modules_affile_tests_test_directory_monitor_LDADD   = $(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/affile/libaffile.la
modules_affile_tests_test_directory_monitor_LDFLAGS = $(PREOPEN_CORE)

modules_affile_tests_test_collection_comporator_CFLAGS  = $(TEST_CFLAGS) -I$(top_srcdir)/modules/affile
modules_affile_tests_test_collection_comporator_LDADD   = $(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/affile/libaffile.la
modules_affile_tests_test_collection_comporator_LDFLAGS = $(PREOPEN_CORE)
