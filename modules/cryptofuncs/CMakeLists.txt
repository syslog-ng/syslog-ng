set (CRYPTOFUNCS_SOURCES
    cryptofuncs.c
)

add_module(
  TARGET cryptofuncs
  DEPENDS OpenSSL::SSL
          OpenSSL::Crypto
  SOURCES ${CRYPTOFUNCS_SOURCES}
)

add_test_subdirectory(tests)

