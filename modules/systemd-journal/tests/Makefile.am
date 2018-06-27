if ENABLE_JOURNALD
modules_systemd_journal_tests_TESTS	= modules/systemd-journal/tests/test_systemd_journal

check_PROGRAMS					+= ${modules_systemd_journal_tests_TESTS}

modules_systemd_journal_tests_test_systemd_journal_SOURCES = \
	modules/systemd-journal/tests/test_systemd_journal.c \
	modules/systemd-journal/tests/journald-mock.c \
	modules/systemd-journal/tests/journald-mock.h \
	modules/systemd-journal/tests/test-source.h \
	modules/systemd-journal/tests/test-source.c

modules_systemd_journal_tests_test_systemd_journal_CFLAGS = $(TEST_CFLAGS) $(libsystemd_CFLAGS) -I$(top_srcdir)/modules/systemd-journal
modules_systemd_journal_tests_test_systemd_journal_LDADD = $(TEST_LDADD) $(IVYKIS_LIBS)
endif

EXTRA_DIST += modules/systemd-journal/tests/CMakeLists.txt
