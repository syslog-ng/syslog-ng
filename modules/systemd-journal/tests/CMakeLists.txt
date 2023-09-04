add_unit_test(CRITERION
  TARGET test_systemd_journal
  DEPENDS sdjournal
  SOURCES test_systemd_journal.c journald-mock.c test-source.c)

add_unit_test(CRITERION
  TARGET test_journald_mock
  DEPENDS sdjournal
  SOURCES test-journald-mock.c journald-mock.c)
