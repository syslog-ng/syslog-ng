modules_tagsparser_tests_TESTS			= modules/tagsparser/tests/test_tagsparser

check_PROGRAMS					+=	\
	${modules_tagsparser_tests_TESTS}

modules_tagsparser_tests_test_tagsparser_CFLAGS	=	\
	$(TEST_CFLAGS) -I$(top_srcdir)/modules/tagsparser
modules_tagsparser_tests_test_tagsparser_LDADD	=	\
	$(TEST_LDADD)
modules_tagsparser_tests_test_tagsparser_LDFLAGS	=	\
	$(PREOPEN_SYSLOGFORMAT)				\
	-dlpreopen $(top_builddir)/modules/tagsparser/libtags-parser.la
