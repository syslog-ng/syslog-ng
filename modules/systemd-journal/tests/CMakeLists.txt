add_unit_test(LIBTEST
  TARGET test_systemd_journal
  DEPENDS systemd
  SOURCES test_systemd_journal.c journald-mock.c test-source.c)
