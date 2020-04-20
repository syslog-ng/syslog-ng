set (BASICFUNCS_SOURCES
  basic-funcs.c
)

add_module(
  TARGET basicfuncs
  SOURCES ${BASICFUNCS_SOURCES}
  DEPENDS m
)

add_test_subdirectory(tests)
