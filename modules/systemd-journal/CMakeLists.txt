option (ENABLE_JOURNALD "Enable systemd-journal" ON)

set (JOURNALD_SOURCES
    systemd-journal.c
    systemd-journal.h
    systemd-journal-grammar.y
    systemd-journal-parser.c
    systemd-journal-parser.h
    systemd-journal-plugin.c
    journal-reader.c
    journal-reader.h
    journald-subsystem.c
    journald-subsystem.h
    journald-helper.c
    ${CMAKE_CURRENT_BINARY_DIR}/systemd-journal-grammar.c
    ${CMAKE_CURRENT_BINARY_DIR}/systemd-journal-grammar.h
)

generate_y_from_ym(modules/systemd-journal/systemd-journal-grammar)

bison_target(SystemdJournalGrammar
  ${CMAKE_CURRENT_BINARY_DIR}/systemd-journal-grammar.y
  ${CMAKE_CURRENT_BINARY_DIR}/systemd-journal-grammar.c
COMPILE_FLAGS ${BISON_FLAGS})

find_package(systemd REQUIRED)

if (ENABLE_JOURNALD AND Libsystemd_FOUND)
    set(BUILD_JOURNALD TRUE)
else()
    set(BUILD_JOURNALD FALSE)
endif()

if (BUILD_JOURNALD)
    include_directories (${CMAKE_CURRENT_BINARY_DIR})
    include_directories (${CMAKE_CURRENT_SOURCE_DIR})
    add_library(sdjournal MODULE ${JOURNALD_SOURCES})
    target_link_libraries(sdjournal PUBLIC ${Libsystemd_LIBRARIES})
    target_link_libraries(sdjournal PUBLIC syslog-ng)
    target_include_directories(sdjournal PRIVATE SYSTEM ${Libsystemd_INCLUDE_DIRS})

    install(TARGETS sdjournal LIBRARY DESTINATION lib/syslog-ng/)

    add_subdirectory(tests)
endif()
