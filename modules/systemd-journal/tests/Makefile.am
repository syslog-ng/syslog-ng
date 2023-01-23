if ENABLE_JOURNALD
modules_systemd_journal_tests_TESTS	= \
	modules/systemd-journal/tests/test_systemd_journal \
	modules/systemd-journal/tests/test_journald_mock

check_PROGRAMS					+= ${modules_systemd_journal_tests_TESTS}

modules_systemd_journal_tests_test_systemd_journal_SOURCES = \
	modules/systemd-journal/tests/test_systemd_journal.c \
	modules/systemd-journal/tests/journald-mock.c \
	modules/systemd-journal/tests/journald-mock.h \
	modules/systemd-journal/tests/test-source.h \
	modules/systemd-journal/tests/test-source.c

modules_systemd_journal_tests_test_systemd_journal_CFLAGS = $(TEST_CFLAGS) $(libsystemd_CFLAGS) -I$(top_srcdir)/modules/systemd-journal
modules_systemd_journal_tests_test_systemd_journal_LDADD = $(TEST_LDADD) $(IVYKIS_LIBS)

modules_systemd_journal_tests_test_journald_mock_SOURCES = \
	modules/systemd-journal/tests/test-journald-mock.c \
	modules/systemd-journal/tests/journald-mock.c \
	modules/systemd-journal/tests/journald-mock.h

modules_systemd_journal_tests_test_journald_mock_CFLAGS = $(TEST_CFLAGS) -I$(top_srcdir)/modules/systemd-journal
modules_systemd_journal_tests_test_journald_mock_LDADD = $(TEST_LDADD)

endif

EXTRA_DIST += modules/systemd-journal/tests/CMakeLists.txt
