modules_csvparser_tests_TESTS			=	\
	modules/csvparser/tests/test_csvparser

check_PROGRAMS					+=	\
	${modules_csvparser_tests_TESTS}

modules_csvparser_tests_test_csvparser_CFLAGS	=	\
	$(TEST_CFLAGS) -I$(top_srcdir)/modules/csvparser
modules_csvparser_tests_test_csvparser_LDADD	=	\
	$(TEST_LDADD)
modules_csvparser_tests_test_csvparser_LDFLAGS	=	\
	-dlpreopen $(top_builddir)/modules/syslogformat/libsyslogformat.la \
	-dlpreopen $(top_builddir)/modules/csvparser/libcsvparser.la
