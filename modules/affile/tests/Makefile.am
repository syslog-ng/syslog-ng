modules_affile_tests_TESTS				= \
	modules/affile/tests/test_affile_open_file

  modules_affile_tests_TESTS += modules/affile/tests/test_affile_capability

  PREOPEN_AFFILE   = -dlpreopen $(top_builddir)/modules/affile/libaffile.la
  PREOPEN_AFSOCKET = -dlpreopen $(top_builddir)/modules/afsocket/libafsocket.la

  modules_affile_tests_test_affile_capability_CFLAGS  = $(TEST_CFLAGS)
  modules_affile_tests_test_affile_capability_LDADD   = $(TEST_LDADD) $(PREOPEN_AFFILE) $(PREOPEN_AFSOCKET)
  modules_affile_tests_test_affile_capability_LDFLAGS = $(PREOPEN_CORE)
  modules_affile_tests_test_affile_capability_SOURCES = modules/affile/tests/test_affile_capability.c

check_PROGRAMS            += \
  ${modules_affile_tests_TESTS}

modules_affile_tests_test_affile_open_file_CFLAGS   = $(TEST_CFLAGS)
modules_affile_tests_test_affile_open_file_LDADD  = $(TEST_LDADD) \
  -dlpreopen $(top_builddir)/modules/affile/libaffile.la
modules_affile_tests_test_affile_open_file_LDFLAGS  =   \
  $(PREOPEN_CORE)
