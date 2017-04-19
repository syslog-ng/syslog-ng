modules_stardate_tests_TESTS			= \
	modules/stardate/tests/test_stardate

check_PROGRAMS					+= ${modules_stardate_tests_TESTS}

modules_stardate_tests_test_stardate_CFLAGS	= $(TEST_CFLAGS) -I$(top_builddir)/modules -lm
modules_stardate_tests_test_stardate_LDADD	= \
	$(TEST_LDADD)				  \
	$(PREOPEN_SYSLOGFORMAT) -dlpreopen $(top_builddir)/modules/stardate/libstardate.la
