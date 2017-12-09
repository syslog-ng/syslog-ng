add_unit_test(LIBTEST
  TARGET test-transport-mapper
  DEPENDS afsocket
  SOURCES test-transport-mapper.c transport-mapper-lib.c)

add_unit_test(LIBTEST
  TARGET test-transport-mapper-inet
  DEPENDS afsocket
  SOURCES test-transport-mapper-inet.c transport-mapper-lib.c)

add_unit_test(LIBTEST
  TARGET test-transport-mapper-unix
  DEPENDS afsocket
  SOURCES test-transport-mapper-unix.c transport-mapper-lib.c)
