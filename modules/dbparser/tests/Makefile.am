modules_dbparser_tests_TESTS			=	\
	modules/dbparser/tests/test_timer_wheel		\
	modules/dbparser/tests/test_patternize		\
	modules/dbparser/tests/test_patterndb		\
	modules/dbparser/tests/test_radix

check_PROGRAMS					+=	\
	${modules_dbparser_tests_TESTS}

modules_dbparser_tests_test_timer_wheel_CFLAGS	=	\
	$(TEST_CFLAGS)					\
	-I$(top_srcdir)/modules/dbparser
modules_dbparser_tests_test_timer_wheel_LDADD	=	\
	$(TEST_LDADD)					\
	$(top_builddir)/modules/dbparser/libsyslog-ng-patterndb.la
modules_dbparser_tests_test_timer_wheel_LDFLAGS	=	\
	-dlpreopen $(top_builddir)/modules/syslogformat/libsyslogformat.la \
	-dlpreopen $(top_builddir)/modules/basicfuncs/libbasicfuncs.la

modules_dbparser_tests_test_patternize_CFLAGS	=	\
	$(TEST_CFLAGS)					\
	-I$(top_srcdir)/modules/dbparser
modules_dbparser_tests_test_patternize_LDADD	=	\
	$(TEST_LDADD) $(CRYPTO_LIBS)			\
	$(top_builddir)/modules/dbparser/libsyslog-ng-patterndb.la
modules_dbparser_tests_test_patternize_LDFLAGS	=	\
	-dlpreopen $(top_builddir)/modules/syslogformat/libsyslogformat.la \
	-dlpreopen $(top_builddir)/modules/basicfuncs/libbasicfuncs.la

modules_dbparser_tests_test_patterndb_CFLAGS	=	\
	$(TEST_CFLAGS)					\
	-I$(top_srcdir)/modules/dbparser
modules_dbparser_tests_test_patterndb_LDADD	=	\
	$(TEST_LDADD)					\
	$(top_builddir)/modules/dbparser/libsyslog-ng-patterndb.la
modules_dbparser_tests_test_patterndb_LDFLAGS	=	\
	-dlpreopen $(top_builddir)/modules/syslogformat/libsyslogformat.la \
	-dlpreopen $(top_builddir)/modules/basicfuncs/libbasicfuncs.la

modules_dbparser_tests_test_radix_CFLAGS	=	\
	$(TEST_CFLAGS)					\
	-I$(top_srcdir)/modules/dbparser		\
	@CFLAGS_NOWARN_POINTER_SIGN@
modules_dbparser_tests_test_radix_LDADD		=	\
	$(TEST_LDADD)					\
	$(top_builddir)/modules/dbparser/libsyslog-ng-patterndb.la
modules_dbparser_tests_test_radix_LDFLAGS	=	\
	-dlpreopen $(top_builddir)/modules/syslogformat/libsyslogformat.la \
	-dlpreopen $(top_builddir)/modules/basicfuncs/libbasicfuncs.la
