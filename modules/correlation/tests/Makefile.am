EXTRA_DIST  +=  modules/correlation/tests/CMakeLists.txt

modules_correlation_tests_TESTS			=	\
	modules/correlation/tests/test_timer_wheel		\
	modules/correlation/tests/test_patternize		\
	modules/correlation/tests/test_patterndb		\
	modules/correlation/tests/test_parsers_e2e		\
	modules/correlation/tests/test_radix		\
	modules/correlation/tests/test_parsers		\
	modules/correlation/tests/test_grouping_by

check_PROGRAMS					+=	\
	${modules_correlation_tests_TESTS}

modules_correlation_tests_test_timer_wheel_CFLAGS	=	\
	$(TEST_CFLAGS)					\
	-I$(top_srcdir)/modules/correlation
modules_correlation_tests_test_timer_wheel_LDADD	=	\
	$(TEST_LDADD)					\
	$(top_builddir)/modules/correlation/libsyslog-ng-patterndb.la
modules_correlation_tests_test_timer_wheel_LDFLAGS	=	\
	$(PREOPEN_CORE)

modules_correlation_tests_test_patternize_CFLAGS	=	\
	$(TEST_CFLAGS)					\
	-I$(top_srcdir)/modules/correlation
modules_correlation_tests_test_patternize_LDADD	=	\
	$(TEST_LDADD)					\
	$(top_builddir)/modules/correlation/libsyslog-ng-patterndb.la
modules_correlation_tests_test_patternize_LDFLAGS	=	\
	$(PREOPEN_CORE)

modules_correlation_tests_test_patterndb_CFLAGS	=	\
	$(TEST_CFLAGS)					\
	-I$(top_srcdir)/modules/correlation
modules_correlation_tests_test_patterndb_SOURCES	=	\
	modules/correlation/tests/test_patterndb.c \
	modules/correlation/tests/test_patterndb.h
modules_correlation_tests_test_patterndb_LDADD	=	\
	$(TEST_LDADD)					\
	$(top_builddir)/modules/correlation/libsyslog-ng-patterndb.la
modules_correlation_tests_test_patterndb_LDFLAGS	=	\
	$(PREOPEN_CORE)

modules_correlation_tests_test_parsers_e2e_CFLAGS	=	\
	$(TEST_CFLAGS)					\
	-I$(top_srcdir)/modules/correlation
modules_correlation_tests_test_parsers_e2e_LDADD	=	\
	$(TEST_LDADD)					\
	$(top_builddir)/modules/correlation/libsyslog-ng-patterndb.la
modules_correlation_tests_test_parsers_e2e_LDFLAGS	=	\
	$(PREOPEN_CORE)

modules_correlation_tests_test_radix_CFLAGS	=	\
	$(TEST_CFLAGS)					\
	-I$(top_srcdir)/modules/correlation
modules_correlation_tests_test_radix_LDADD		=	\
	$(TEST_LDADD)					\
	$(top_builddir)/modules/correlation/libsyslog-ng-patterndb.la
modules_correlation_tests_test_radix_LDFLAGS	=	\
	$(PREOPEN_CORE)

modules_correlation_tests_test_parsers_CFLAGS	=	\
	$(TEST_CFLAGS)					\
	-I$(top_srcdir)/modules/correlation
modules_correlation_tests_test_parsers_LDADD		=	\
	$(TEST_LDADD)					\
	$(top_builddir)/modules/correlation/libsyslog-ng-patterndb.la
modules_correlation_tests_test_parsers_LDFLAGS	=	\
	$(PREOPEN_CORE)

modules_correlation_tests_test_grouping_by_CFLAGS	=	\
	$(TEST_CFLAGS)					\
	-I$(top_srcdir)/modules/correlation
modules_correlation_tests_test_grouping_by_LDADD		=	\
	$(TEST_LDADD)					\
	$(top_builddir)/modules/correlation/libcorrelation.la
modules_correlation_tests_test_grouping_by_LDFLAGS	=	\
	$(PREOPEN_CORE)					\
	-dlpreopen $(top_builddir)/modules/correlation/libcorrelation.la
