EXTRA_DIST  +=  modules/dbparser/tests/CMakeLists.txt

modules_dbparser_tests_TESTS			=	\
	modules/dbparser/tests/test_timer_wheel		\
	modules/dbparser/tests/test_patternize		\
	modules/dbparser/tests/test_patterndb		\
	modules/dbparser/tests/test_parsers_e2e		\
	modules/dbparser/tests/test_radix		\
	modules/dbparser/tests/test_parsers

check_PROGRAMS					+=	\
	${modules_dbparser_tests_TESTS}

modules_dbparser_tests_test_timer_wheel_CFLAGS	=	\
	$(TEST_CFLAGS)					\
	-I$(top_srcdir)/modules/dbparser
modules_dbparser_tests_test_timer_wheel_LDADD	=	\
	$(TEST_LDADD)					\
	$(top_builddir)/modules/dbparser/libsyslog-ng-patterndb.la
modules_dbparser_tests_test_timer_wheel_LDFLAGS	=	\
	$(PREOPEN_CORE)

modules_dbparser_tests_test_patternize_CFLAGS	=	\
	$(TEST_CFLAGS)					\
	-I$(top_srcdir)/modules/dbparser
modules_dbparser_tests_test_patternize_LDADD	=	\
	$(TEST_LDADD)					\
	$(top_builddir)/modules/dbparser/libsyslog-ng-patterndb.la
modules_dbparser_tests_test_patternize_LDFLAGS	=	\
	$(PREOPEN_CORE)

modules_dbparser_tests_test_patterndb_CFLAGS	=	\
	$(TEST_CFLAGS)					\
	-I$(top_srcdir)/modules/dbparser
modules_dbparser_tests_test_patterndb_SOURCES	=	\
	modules/dbparser/tests/test_patterndb.c \
	modules/dbparser/tests/test_patterndb.h
modules_dbparser_tests_test_patterndb_LDADD	=	\
	$(TEST_LDADD)					\
	$(top_builddir)/modules/dbparser/libsyslog-ng-patterndb.la
modules_dbparser_tests_test_patterndb_LDFLAGS	=	\
	$(PREOPEN_CORE)

modules_dbparser_tests_test_parsers_e2e_CFLAGS	=	\
	$(TEST_CFLAGS)					\
	-I$(top_srcdir)/modules/dbparser
modules_dbparser_tests_test_parsers_e2e_LDADD	=	\
	$(TEST_LDADD)					\
	$(top_builddir)/modules/dbparser/libsyslog-ng-patterndb.la
modules_dbparser_tests_test_parsers_e2e_LDFLAGS	=	\
	$(PREOPEN_CORE)

modules_dbparser_tests_test_radix_CFLAGS	=	\
	$(TEST_CFLAGS)					\
	-I$(top_srcdir)/modules/dbparser
modules_dbparser_tests_test_radix_LDADD		=	\
	$(TEST_LDADD)					\
	$(top_builddir)/modules/dbparser/libsyslog-ng-patterndb.la
modules_dbparser_tests_test_radix_LDFLAGS	=	\
	$(PREOPEN_CORE)

modules_dbparser_tests_test_parsers_CFLAGS	=	\
	$(TEST_CFLAGS)					\
	-I$(top_srcdir)/modules/dbparser
modules_dbparser_tests_test_parsers_LDADD		=	\
	$(TEST_LDADD)					\
	$(top_builddir)/modules/dbparser/libsyslog-ng-patterndb.la
modules_dbparser_tests_test_parsers_LDFLAGS	=	\
	$(PREOPEN_CORE)
