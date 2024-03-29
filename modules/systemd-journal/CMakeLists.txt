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
    journald-helper.h
    ${CMAKE_CURRENT_BINARY_DIR}/systemd-journal-grammar.c
    ${CMAKE_CURRENT_BINARY_DIR}/systemd-journal-grammar.h
)

generate_y_from_ym(modules/systemd-journal/systemd-journal-grammar)

bison_target(SystemdJournalGrammar
  ${CMAKE_CURRENT_BINARY_DIR}/systemd-journal-grammar.y
  ${CMAKE_CURRENT_BINARY_DIR}/systemd-journal-grammar.c
COMPILE_FLAGS ${BISON_FLAGS})
set_source_files_properties(
  ${CMAKE_CURRENT_BINARY_DIR}/systemd-journal-grammar.c
  PROPERTIES
  COMPILE_FLAGS ${BISON_BUILT_SOURCE_CFLAGS}
)

if (ENABLE_JOURNALD)
    add_library(sdjournal SHARED ${JOURNALD_SOURCES})
    target_include_directories(sdjournal
      PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
      PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
    )
    target_link_libraries(sdjournal PRIVATE ${Libsystemd_LIBRARIES})
    target_link_libraries(sdjournal PRIVATE syslog-ng)
    target_include_directories(sdjournal PRIVATE SYSTEM ${Libsystemd_INCLUDE_DIRS})

    install(TARGETS sdjournal LIBRARY DESTINATION lib/syslog-ng/)

    add_test_subdirectory(tests)
endif()
