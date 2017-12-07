set(TEST_LOGPROTO_SOURCES
  test_logproto.c
  test_logproto.h
  test-server-options.c
  test-record-server.c
  test-text-server.c
  test-dgram-server.c
  test-framed-server.c
  test-indented-multiline-server.c
  test-regexp-multiline-server.c)

add_unit_test(LIBTEST
  TARGET test_logproto
  SOURCES "${TEST_LOGPROTO_SOURCES}")

add_unit_test(TARGET test_findeom)
