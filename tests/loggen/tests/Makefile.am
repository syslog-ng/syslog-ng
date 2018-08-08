tests_loggen_tests_test_loggen_filereader_TESTS			=	\
	tests/loggen/tests/test_loggen_filereader

check_PROGRAMS					+=	\
	${tests_loggen_tests_test_loggen_filereader_TESTS}

tests_loggen_tests_test_loggen_filereader_CFLAGS	=	\
	$(TEST_CFLAGS) -I$(top_srcdir)/tests/loggen

tests_loggen_tests_test_loggen_filereader_LDADD	=	\
	$(TEST_LDADD) \
	tests/loggen/libloggen_helper.la

tests_loggen_tests_test_loggen_filereader_LDFLAGS	=	\
	$(PREOPEN_SYSLOGFORMAT)
