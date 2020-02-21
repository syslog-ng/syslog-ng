find_package(NetSnmp)

module_switch(ENABLE_SNMP_DEST "Enable snmp destination driver" NETSNMP_FOUND)

if (NOT ENABLE_SNMP_DEST)
  return ()
endif()

if (NOT NETSNMP_FOUND)
  message(FATAL_ERROR "SNMP destination module enabled, but Net-SNMP not found")
endif()

set(snmpdest_SOURCES
    "snmpdest-parser.c"
    "snmpdest.c"
    "snmpdest.h"
    "snmpdest-parser.c"
    "snmpdest-plugin.c"
    "snmpdest-grammar.y"
)

add_module(
  TARGET snmpdest
  GRAMMAR snmpdest-grammar
  DEPENDS ${NETSNMP_LIBS}
  SOURCES ${snmpdest_SOURCES}
)

set_target_properties(snmpdest PROPERTIES COMPILE_FLAGS ${NETSNMP_CFLAGS})

add_test_subdirectory(tests)

