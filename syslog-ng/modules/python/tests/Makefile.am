PYTHONMALLOC=malloc_debug
export PYTHONMALLOC

check_PROGRAMS += \
  ${modules_python_tests_TESTS}

modules_python_tests_TESTS = \
  modules/python/tests/test_python_logmsg \
  modules/python/tests/test_python_template \
  modules/python/tests/test_python_persist_name

modules_python_tests_test_python_logmsg_CFLAGS = $(TEST_CFLAGS) $(PYTHON_CFLAGS) -I$(top_srcdir)/modules/python
modules_python_tests_test_python_logmsg_LDADD = $(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/python/libmod-python.la \
	$(PREOPEN_SYSLOGFORMAT) \
	$(PYTHON_LIBS)

modules_python_tests_test_python_template_CFLAGS = $(TEST_CFLAGS) $(PYTHON_CFLAGS) \
	-I$(top_srcdir)/modules/python -I$(top_srcdir)/modules/syslogformat
modules_python_tests_test_python_template_LDADD = $(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/python/libmod-python.la \
	$(PYTHON_LIBS) $(PREOPEN_SYSLOGFORMAT)

modules_python_tests_test_python_persist_name_CFLAGS = $(TEST_CFLAGS) $(PYTHON_CFLAGS) \
	-I$(top_srcdir)/modules/python -I$(top_srcdir)/modules/syslogformat
modules_python_tests_test_python_persist_name_LDADD = $(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/python/libmod-python.la \
	$(PYTHON_LIBS) $(PREOPEN_SYSLOGFORMAT)

EXTRA_DIST += modules/python/tests/CMakeLists.txt
