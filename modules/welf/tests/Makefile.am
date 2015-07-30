modules_welf_tests_TESTS		= \
	modules/welf/tests/test_format_welf

check_PROGRAMS				+= ${modules_welf_tests_TESTS}

modules_welf_tests_test_format_welf_CFLAGS	= $(TEST_CFLAGS)
modules_welf_tests_test_format_welf_LDADD	= $(TEST_LDADD)
modules_welf_tests_test_format_welf_LDFLAGS	= \
	$(PREOPEN_SYSLOGFORMAT)		  \
	-dlpreopen $(top_builddir)/modules/welf/libwelf.la
modules_welf_tests_test_format_welf_DEPENDENCIES = $(top_builddir)/modules/welf/libwelf.la
