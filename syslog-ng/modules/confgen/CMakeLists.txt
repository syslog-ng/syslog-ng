set (CONFGEN_SOURCES
    confgen.h
    confgen-plugin.c
)

add_module(
  TARGET confgen
  SOURCES ${CONFGEN_SOURCES}
)

add_test_subdirectory(tests)
