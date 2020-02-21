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
    "afsnmp-grammar.y"
    "${CMAKE_CURRENT_BINARY_DIR}/afsnmp-grammar.c"
)

generate_y_from_ym(modules/afsnmp/afsnmp-grammar)

bison_target(afsnmp_GRAMMAR
    ${CMAKE_CURRENT_BINARY_DIR}/afsnmp-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/afsnmp-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

add_library(afsnmp SHARED ${afsnmp_SOURCES})

target_include_directories(afsnmp
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)

set_target_properties(afsnmp PROPERTIES COMPILE_FLAGS ${NETSNMP_CFLAGS})

target_link_libraries(afsnmp PRIVATE ${NETSNMP_LIBS})
target_link_libraries(afsnmp PRIVATE syslog-ng)

add_test_subdirectory(tests)

install(TARGETS afsnmp LIBRARY DESTINATION lib/syslog-ng/ COMPONENT afsnmp)
