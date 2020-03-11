find_package(NetSnmp)

module_switch(ENABLE_AFSNMP "Enable afsnmp module" NETSNMP_FOUND)

if (NOT ENABLE_AFSNMP)
  return ()
endif()

if (NOT NETSNMP_FOUND)
  message(FATAL_ERROR "SNMP module enabled, but Net-SNMP not found")
endif()

set(afsnmp_SOURCES
    "afsnmpdest.h"
    "afsnmpdest.c"
    "afsnmp-parser.h"
    "afsnmp-parser.c"
    "afsnmp-plugin.c"
    "snmptrapd-header-parser.h"
    "snmptrapd-header-parser.c"
    "snmptrapd-nv-context.h"
    "snmptrapd-parser.h"
    "snmptrapd-parser.c"
    "varbindlist-scanner.h"
    "varbindlist-scanner.c"
)

add_module(
  TARGET afsnmp
  GRAMMAR afsnmp-grammar
  DEPENDS ${NETSNMP_LIBS}
  SOURCES ${afsnmp_SOURCES}
)

set_target_properties(afsnmp PROPERTIES COMPILE_FLAGS ${NETSNMP_CFLAGS})

add_test_subdirectory(tests)

