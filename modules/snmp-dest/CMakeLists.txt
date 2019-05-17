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
    "${CMAKE_CURRENT_BINARY_DIR}/snmpdest-grammar.c"
)

generate_y_from_ym(modules/snmp-dest/snmpdest-grammar)

bison_target(snmpdest_GRAMMAR
    ${CMAKE_CURRENT_BINARY_DIR}/snmpdest-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/snmpdest-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

add_library(snmpdest SHARED ${snmpdest_SOURCES})

target_include_directories(snmpdest
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_definitions(snmpdest PRIVATE NETSNMP_CFLAGS)

target_link_libraries(snmpdest PRIVATE ${NETSNMP_LIBS})
target_link_libraries(snmpdest PRIVATE syslog-ng)

add_test_subdirectory(tests)

install(TARGETS snmpdest LIBRARY DESTINATION lib/syslog-ng/ COMPONENT snmpdest)
