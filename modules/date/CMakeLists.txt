set(DATE_PARSER_SOURCES
    date-plugin.c
    date-parser.c
    date-parser.h
    date-parser-parser.c
    date-parser-parser.h
    ${CMAKE_CURRENT_BINARY_DIR}/date-grammar.c
    ${CMAKE_CURRENT_BINARY_DIR}/date-grammar.h
)

generate_y_from_ym(modules/date/date-grammar)

bison_target(DateParserGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/date-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/date-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

add_library(date SHARED ${DATE_PARSER_SOURCES})
target_include_directories(date
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(date PRIVATE syslog-ng)

install(TARGETS date LIBRARY DESTINATION lib/syslog-ng/ COMPONENT date)

add_test_subdirectory(tests)
