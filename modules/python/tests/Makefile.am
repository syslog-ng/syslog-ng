check_PROGRAMS += \
  ${modules_python_tests_TESTS}

modules_python_tests_TESTS = \
  modules/python/tests/test_python_logmsg

modules_python_tests_test_python_logmsg_CFLAGS = $(TEST_CFLAGS) $(PYTHON_CFLAGS) -I$(top_srcdir)/modules/python
modules_python_tests_test_python_logmsg_LDADD = $(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/python/libmod-python.la \
	$(PYTHON_LIBS)
