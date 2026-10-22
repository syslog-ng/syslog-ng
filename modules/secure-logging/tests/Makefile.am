# File: syslog-ng/modules/secure-logging/tests/Makefile.am

if ENABLE_SLOG

modules_secure_logging_tests_TESTS = \
	modules/secure-logging/tests/test_secure_logging

check_PROGRAMS += \
	${modules_secure_logging_tests_TESTS}

modules_secure_logging_tests_test_secure_logging_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/modules/secure-logging \
	-I$(top_builddir)/modules/secure-logging

modules_secure_logging_tests_test_secure_logging_CFLAGS = \
	$(AM_CFLAGS) \
	$(TEST_CFLAGS)

modules_secure_logging_tests_test_secure_logging_LDADD = \
	$(TEST_LDADD) \
	-dlpreopen ${top_builddir}/modules/secure-logging/libsecure-logging.la \
	-dlpreopen ${top_builddir}/modules/syslogformat/libsyslogformat.la

endif

EXTRA_DIST += modules/secure-logging/tests/CMakeLists.txt

