modules_basicfuncs_tests_TESTS			= modules/basicfuncs/tests/test_basicfuncs
check_PROGRAMS					+= ${modules_basicfuncs_tests_TESTS}

modules_basicfuncs_tests_test_basicfuncs_CFLAGS	= $(TEST_CFLAGS)
modules_basicfuncs_tests_test_basicfuncs_LDADD	= \
	$(TEST_LDADD)				  \
	-dlpreopen ${top_builddir}/modules/syslogformat/libsyslogformat.la \
	-dlpreopen ${top_builddir}/modules/basicfuncs/libbasicfuncs.la
