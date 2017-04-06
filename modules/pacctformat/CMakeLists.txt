set(PACCT_FORMAT_SOURCES
  pacct-format.c
  pacct-format.h
  pacct-format-plugin.c
)

if (CMAKE_SYSTEM_NAME STREQUAL Linux)
    option(ENABLE_PACCT "Enable support for reading Process Accounting files (EXPERIMENTAL, Linux only)." ON)
else()
    option(ENABLE_PACCT "Enable support for reading Process Accounting files (EXPERIMENTAL, Linux only)." OFF)
endif()

if (ENABLE_PACCT)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR})
    add_library(pacctformat MODULE ${PACCT_FORMAT_SOURCES})
    target_link_libraries(pacctformat PRIVATE syslog-ng)

    install(TARGETS pacctformat LIBRARY DESTINATION lib/syslog-ng/ COMPONENT pacctformat)
endif()
