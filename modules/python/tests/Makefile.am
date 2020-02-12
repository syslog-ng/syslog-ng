PYTHONMALLOC=malloc_debug
export PYTHONMALLOC

modules_python_tests_TESTS =

check_PROGRAMS += \
  ${modules_python_tests_TESTS}

python_test_common_cflags = $(TEST_CFLAGS) -I$(top_srcdir)/modules/python

if ENABLE_PYTHON2
preopen_python2 = -dlpreopen $(top_builddir)/modules/python/libmod-python2.la

modules_python_tests_TESTS += \
  modules/python/tests/test_python2_logmsg \
  modules/python/tests/test_python2_template \
  modules/python/tests/test_python2_persist_name

modules_python_tests_test_python2_logmsg_SOURCES = modules/python/tests/test_python_logmsg.c
modules_python_tests_test_python2_logmsg_CPPFLAGS = $(PYTHON2_CPPFLAGS) $(AM_CPPFLAGS)
modules_python_tests_test_python2_logmsg_CFLAGS = $(python_test_common_cflags) $(PYTHON2_CFLAGS)
modules_python_tests_test_python2_logmsg_LDADD = $(TEST_LDADD) $(preopen_python2) $(PREOPEN_SYSLOGFORMAT) $(PYTHON2_LIBS)

modules_python_tests_test_python2_template_SOURCES = modules/python/tests/test_python_template.c
modules_python_tests_test_python2_template_CPPFLAGS = $(PYTHON2_CPPFLAGS) $(AM_CPPFLAGS)
modules_python_tests_test_python2_template_CFLAGS = $(python_test_common_cflags) $(PYTHON2_CFLAGS) \
	-I$(top_srcdir)/modules/syslogformat
modules_python_tests_test_python2_template_LDADD = $(TEST_LDADD) $(preopen_python2) $(PYTHON2_LIBS) $(PREOPEN_SYSLOGFORMAT)

modules_python_tests_test_python2_persist_name_SOURCES = modules/python/tests/test_python_persist_name.c
modules_python_tests_test_python2_persist_name_CPPFLAGS = $(PYTHON2_CPPFLAGS) $(AM_CPPFLAGS)
modules_python_tests_test_python2_persist_name_CFLAGS = $(python_test_common_cflags) $(PYTHON2_CFLAGS) \
	-I$(top_srcdir)/modules/syslogformat
modules_python_tests_test_python2_persist_name_LDADD = $(TEST_LDADD) $(preopen_python2) $(PYTHON2_LIBS) $(PREOPEN_SYSLOGFORMAT)
endif

if ENABLE_PYTHON3
preopen_python3 = -dlpreopen $(top_builddir)/modules/python/libmod-python3.la

modules_python_tests_TESTS += \
  modules/python/tests/test_python3_logmsg \
  modules/python/tests/test_python3_template \
  modules/python/tests/test_python3_persist_name

modules_python_tests_test_python3_logmsg_SOURCES = modules/python/tests/test_python_logmsg.c
modules_python_tests_test_python3_logmsg_CPPFLAGS = $(PYTHON3_CPPFLAGS) $(AM_CPPFLAGS)
modules_python_tests_test_python3_logmsg_CFLAGS = $(python_test_common_cflags) $(PYTHON3_CFLAGS)
modules_python_tests_test_python3_logmsg_LDADD = $(TEST_LDADD) $(preopen_python3) $(PREOPEN_SYSLOGFORMAT) $(PYTHON3_LIBS)

modules_python_tests_test_python3_template_SOURCES = modules/python/tests/test_python_template.c
modules_python_tests_test_python3_template_CPPFLAGS = $(PYTHON3_CPPFLAGS) $(AM_CPPFLAGS)
modules_python_tests_test_python3_template_CFLAGS = $(python_test_common_cflags) $(PYTHON3_CFLAGS) \
	-I$(top_srcdir)/modules/syslogformat
modules_python_tests_test_python3_template_LDADD = $(TEST_LDADD) $(preopen_python3) $(PYTHON3_LIBS) $(PREOPEN_SYSLOGFORMAT)

modules_python_tests_test_python3_persist_name_SOURCES = modules/python/tests/test_python_persist_name.c
modules_python_tests_test_python3_persist_name_CPPFLAGS = $(PYTHON3_CPPFLAGS) $(AM_CPPFLAGS)
modules_python_tests_test_python3_persist_name_CFLAGS = $(python_test_common_cflags) $(PYTHON3_CFLAGS) \
	-I$(top_srcdir)/modules/syslogformat
modules_python_tests_test_python3_persist_name_LDADD = $(TEST_LDADD) $(preopen_python3) $(PYTHON3_LIBS) $(PREOPEN_SYSLOGFORMAT)
endif

EXTRA_DIST += modules/python/tests/CMakeLists.txt
