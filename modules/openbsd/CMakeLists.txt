if (CMAKE_SYSTEM_NAME EQUAL "OpenBSD")
  set(IS_OPENBSD "ON")
else()
  set(IS_OPENBSD "OFF")
endif()
module_switch(ENABLE_OPENBSD_SYS_DRIVER "Enable openBSD system source driver" IS_OPENBSD)

if (ENABLE_OPENBSD_SYS_DRIVER AND NOT CMAKE_SYSTEM_NAME EQUAL "OpenBSD")
    message(FATAL_ERROR "OpenBSD source was explicitly enabled, but OS is not OpenBSD")
endif()

if (NOT ENABLE_OPENBSD_SYS_DRIVER)
    return()
endif()

set(OPENBSD_SYS_DRIVER_HEADERS
    "openbsd-parser.h"
    "${CMAKE_CURRENT_BINARY_DIR}/openbsd-grammar.h"
)

set(OPENBSD_SYS_DRIVER_SOURCES
    "openbsd-parser.c"
    "openbsd-plugin.c"
    "openbsd-driver.c"
    "openbsd-grammar.y"
    "${CMAKE_CURRENT_BINARY_DIR}/openbsd-grammar.c"
)

generate_y_from_ym(modules/openbsd/openbsd-grammar)

bison_target(openbsdGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/openbsd-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/openbsd-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})
set_source_files_properties(
    ${CMAKE_CURRENT_BINARY_DIR}/openbsd-grammar.c
    PROPERTIES
    COMPILE_FLAGS ${BISON_BUILT_SOURCE_CFLAGS}
)

add_library(openbsd SHARED ${openbsd_SOURCES})
target_include_directories(openbsd
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(openbsd PRIVATE syslog-ng)

install(TARGETS openbsd LIBRARY DESTINATION lib/syslog-ng/ COMPONENT openbsd)
