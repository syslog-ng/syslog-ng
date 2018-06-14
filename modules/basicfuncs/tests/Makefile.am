modules_basicfuncs_tests_TESTS			= \
	modules/basicfuncs/tests/test_basicfuncs

EXTRA_DIST += modules/basicfuncs/tests/CMakeLists.txt

check_PROGRAMS					+= ${modules_basicfuncs_tests_TESTS}

modules_basicfuncs_tests_test_basicfuncs_CFLAGS	= $(TEST_CFLAGS)
modules_basicfuncs_tests_test_basicfuncs_LDADD	= \
	$(TEST_LDADD)				  \
	$(PREOPEN_SYSLOGFORMAT) $(PREOPEN_BASICFUNCS)
