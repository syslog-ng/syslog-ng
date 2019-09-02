modules_timestamp_tests_TESTS = \
	modules/timestamp/tests/test_date

check_PROGRAMS += \
	${modules_timestamp_tests_TESTS}

EXTRA_DIST += modules/timestamp/tests/CMakeLists.txt

modules_timestamp_tests_test_date_CFLAGS = $(TEST_CFLAGS) -I$(top_srcdir)/modules/timestamp
modules_timestamp_tests_test_date_LDADD = $(TEST_LDADD)
modules_timestamp_tests_test_date_LDFLAGS = \
	-dlpreopen $(top_builddir)/modules/timestamp/libtimestamp.la
modules_timestamp_tests_test_date_DEPENDENCIES = $(top_builddir)/modules/timestamp/libtimestamp.la
