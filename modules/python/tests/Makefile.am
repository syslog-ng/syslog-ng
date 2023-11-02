if ENABLE_PYTHON

PYTHONMALLOC=malloc_debug
export PYTHONMALLOC

check_PROGRAMS += \
  ${modules_python_tests_TESTS}

modules_python_tests_TESTS = \
  modules/python/tests/test_python_logmsg \
  modules/python/tests/test_python_template \
  modules/python/tests/test_python_tf \
  modules/python/tests/test_python_persist_name \
  modules/python/tests/test_python_persist \
  modules/python/tests/test_python_bookmark \
  modules/python/tests/test_python_ack_tracker \
  modules/python/tests/test_python_options \
  modules/python/tests/test_python_reloc

modules_python_tests_test_python_logmsg_CFLAGS = $(TEST_CFLAGS) $(PYTHON_CFLAGS) -I$(top_srcdir)/modules/python
modules_python_tests_test_python_logmsg_LDADD = $(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/python/libmod-python.la \
	$(PYTHON_LIBS) $(PREOPEN_SYSLOGFORMAT)

modules_python_tests_test_python_template_CFLAGS = $(TEST_CFLAGS) $(PYTHON_CFLAGS) \
	-I$(top_srcdir)/modules/python -I$(top_srcdir)/modules/syslogformat
modules_python_tests_test_python_template_LDADD = $(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/python/libmod-python.la \
	$(PYTHON_LIBS) $(PREOPEN_SYSLOGFORMAT)

modules_python_tests_test_python_tf_CFLAGS = $(TEST_CFLAGS) $(PYTHON_CFLAGS) \
	-I$(top_srcdir)/modules/python -I$(top_srcdir)/modules/syslogformat
modules_python_tests_test_python_tf_LDADD = $(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/python/libmod-python.la \
	$(PYTHON_LIBS) $(PREOPEN_SYSLOGFORMAT)

modules_python_tests_test_python_persist_name_CFLAGS = $(TEST_CFLAGS) $(PYTHON_CFLAGS) \
	-I$(top_srcdir)/modules/python -I$(top_srcdir)/modules/syslogformat
modules_python_tests_test_python_persist_name_LDADD = $(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/python/libmod-python.la \
	$(PYTHON_LIBS) $(PREOPEN_SYSLOGFORMAT)

modules_python_tests_test_python_persist_CFLAGS = $(TEST_CFLAGS) $(PYTHON_CFLAGS) \
	-I$(top_srcdir)/modules/python -I$(top_srcdir)/modules/syslogformat
modules_python_tests_test_python_persist_LDADD = $(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/python/libmod-python.la \
	$(PYTHON_LIBS) $(PREOPEN_SYSLOGFORMAT)

modules_python_tests_test_python_bookmark_CFLAGS = $(TEST_CFLAGS) $(PYTHON_CFLAGS) \
	-I$(top_srcdir)/modules/python -I$(top_srcdir)/modules/syslogformat
modules_python_tests_test_python_bookmark_LDADD = $(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/python/libmod-python.la \
	$(PYTHON_LIBS)

modules_python_tests_test_python_ack_tracker_CFLAGS = $(TEST_CFLAGS) $(PYTHON_CFLAGS) \
	-I$(top_srcdir)/modules/python -I$(top_srcdir)/modules/syslogformat
modules_python_tests_test_python_ack_tracker_LDADD = $(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/python/libmod-python.la \
	$(PYTHON_LIBS) $(PREOPEN_SYSLOGFORMAT)

modules_python_tests_test_python_options_CFLAGS = $(TEST_CFLAGS) $(PYTHON_CFLAGS) \
	-I$(top_srcdir)/modules/python
modules_python_tests_test_python_options_LDADD = $(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/python/libmod-python.la \
	$(PYTHON_LIBS)

modules_python_tests_test_python_reloc_CFLAGS = $(TEST_CFLAGS) $(PYTHON_CFLAGS) \
	-I$(top_srcdir)/modules/python
modules_python_tests_test_python_reloc_LDADD = $(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/python/libmod-python.la \
	$(PYTHON_LIBS)

endif

EXTRA_DIST += modules/python/tests/CMakeLists.txt
