modules_csvparser_tests_TESTS			=	\
	modules/csvparser/tests/test_csvparser		\
	modules/csvparser/tests/test_csvparser_perf

check_PROGRAMS					+=	\
	${modules_csvparser_tests_TESTS}

modules_csvparser_tests_test_csvparser_CFLAGS	=	\
	$(TEST_CFLAGS) -I$(top_srcdir)/modules/csvparser
modules_csvparser_tests_test_csvparser_LDADD	=	\
	$(TEST_LDADD)
modules_csvparser_tests_test_csvparser_LDFLAGS	=	\
	$(PREOPEN_SYSLOGFORMAT)				\
	-dlpreopen $(top_builddir)/modules/csvparser/libcsvparser.la

modules_csvparser_tests_test_csvparser_perf_CFLAGS	=	\
	$(TEST_CFLAGS) -I$(top_srcdir)/modules/csvparser
modules_csvparser_tests_test_csvparser_perf_LDADD	=	\
	$(TEST_LDADD)
modules_csvparser_tests_test_csvparser_perf_LDFLAGS	=	\
	$(PREOPEN_SYSLOGFORMAT)				\
	-dlpreopen $(top_builddir)/modules/csvparser/libcsvparser.la

modules_csvparser_tests_TESTS += modules/csvparser/tests/test_csvparser_statistics

modules_csvparser_tests_test_csvparser_statistics_CFLAGS	=	\
	$(TEST_CFLAGS) -I$(top_srcdir)/modules/csvparser
modules_csvparser_tests_test_csvparser_statistics_LDADD	=	\
	$(TEST_LDADD)
modules_csvparser_tests_test_csvparser_statistics_LDFLAGS	=	\
	$(PREOPEN_SYSLOGFORMAT)				\
	-dlpreopen $(top_builddir)/modules/csvparser/libcsvparser.la
