set(SYSLOGFORMAT_SOURCES
    syslog-format.c
    syslog-format.h
    syslog-timestamp.c
    syslog-timestamp.h
    syslog-format-plugin.c
    syslog-parser-parser.c
    syslog-parser-parser.h
    syslog-parser.c
    syslog-parser.h
    ${CMAKE_CURRENT_BINARY_DIR}/syslog-parser-grammar.c
    ${CMAKE_CURRENT_BINARY_DIR}/syslog-parser-grammar.h
)

generate_y_from_ym(modules/syslogformat/syslog-parser-grammar)

bison_target(SyslogParserGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/syslog-parser-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/syslog-parser-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

add_library(syslogformat SHARED ${SYSLOGFORMAT_SOURCES})
target_include_directories(syslogformat
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(syslogformat PRIVATE syslog-ng)

install(TARGETS syslogformat LIBRARY DESTINATION lib/syslog-ng/)
