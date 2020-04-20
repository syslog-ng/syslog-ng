lib_filter_tests_TESTS		 =              \
		lib/filter/tests/test_filters_facility      \
		lib/filter/tests/test_filters_level_new      \
		lib/filter/tests/test_filter_call           \
		lib/filter/tests/test_filters_in_list		\
		lib/filter/tests/test_filters_regexp \
		lib/filter/tests/test_filters_fop_cmp \
		lib/filter/tests/test_filters_fop		\
		lib/filter/tests/test_filters_netmask

EXTRA_DIST += lib/filter/tests/CMakeLists.txt

if ENABLE_IPV6
lib_filter_tests_TESTS += lib/filter/tests/test_filters_netmask6
endif

check_PROGRAMS				+= ${lib_filter_tests_TESTS}

lib_filter_tests_test_filters_facility_CFLAGS     = $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/filter/tests
lib_filter_tests_test_filters_facility_LDADD      = $(TEST_LDADD)  \
	$(PREOPEN_SYSLOGFORMAT)
lib_filter_tests_test_filters_facility_SOURCES = 			\
	lib/filter/tests/test_filters_facility.c \
	lib/filter/tests/test_filters_common.c \
	lib/filter/tests/test_filters_common.h

lib_filter_tests_test_filters_level_new_CFLAGS     = $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/filter/tests
lib_filter_tests_test_filters_level_new_LDADD      = $(TEST_LDADD)  \
	$(PREOPEN_SYSLOGFORMAT)
lib_filter_tests_test_filters_level_new_SOURCES = 			\
	lib/filter/tests/test_filters_level_new.c \
	lib/filter/tests/test_filters_common.c \
	lib/filter/tests/test_filters_common.h

lib_filter_tests_test_filters_regexp_CFLAGS     = $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/filter/tests
lib_filter_tests_test_filters_regexp_LDADD      = $(TEST_LDADD)  \
	$(PREOPEN_SYSLOGFORMAT)
lib_filter_tests_test_filters_regexp_SOURCES = 			\
	lib/filter/tests/test_filters_regexp.c \
	lib/filter/tests/test_filters_common.c \
	lib/filter/tests/test_filters_common.h

lib_filter_tests_test_filters_fop_cmp_CFLAGS     = $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/filter/tests
lib_filter_tests_test_filters_fop_cmp_LDADD      = $(TEST_LDADD)  \
	$(PREOPEN_SYSLOGFORMAT)
lib_filter_tests_test_filters_fop_cmp_SOURCES = 			\
	lib/filter/tests/test_filters_fop_cmp.c \
	lib/filter/tests/test_filters_common.c \
	lib/filter/tests/test_filters_common.h

lib_filter_tests_test_filters_fop_CFLAGS         = $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/filter/tests
lib_filter_tests_test_filters_fop_LDADD          = $(TEST_LDADD)  \
	$(PREOPEN_SYSLOGFORMAT)
lib_filter_tests_test_filters_fop_SOURCES = 			\
	lib/filter/tests/test_filters_fop.c \
	lib/filter/tests/test_filters_common.c \
	lib/filter/tests/test_filters_common.h

lib_filter_tests_test_filters_netmask_CFLAGS     = $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/filter/tests
lib_filter_tests_test_filters_netmask_LDADD      = $(TEST_LDADD)  \
	$(PREOPEN_SYSLOGFORMAT)
lib_filter_tests_test_filters_netmask_SOURCES = 			\
	lib/filter/tests/test_filters_netmask.c \
	lib/filter/tests/test_filters_common.c \
	lib/filter/tests/test_filters_common.h

lib_filter_tests_test_filters_in_list_CFLAGS     = $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/filter/tests
lib_filter_tests_test_filters_in_list_LDADD      = $(TEST_LDADD)  \
	$(PREOPEN_SYSLOGFORMAT)

if ENABLE_IPV6
lib_filter_tests_test_filters_netmask6_CFLAGS    = $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/filter/tests
lib_filter_tests_test_filters_netmask6_LDADD     = $(TEST_LDADD)  \
	$(PREOPEN_SYSLOGFORMAT)
lib_filter_tests_test_filters_netmask6_SOURCES = 			\
	lib/filter/tests/test_filters_netmask6.c \
	lib/filter/tests/test_filters_common.c \
	lib/filter/tests/test_filters_common.h
endif

lib_filter_tests_TESTS += lib/filter/tests/test_filters_statistics

lib_filter_tests_test_filters_statistics_CFLAGS  = $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/filter/tests
lib_filter_tests_test_filters_statistics_LDADD     = $(TEST_LDADD)  \
	$(PREOPEN_SYSLOGFORMAT)

lib_filter_tests_test_filter_call_CFLAGS  = $(TEST_CFLAGS) \
	-I${top_srcdir}/lib/filter/tests
lib_filter_tests_test_filter_call_LDADD   = $(TEST_LDADD)

include lib/filter/tests/filters-in-list/Makefile.am
