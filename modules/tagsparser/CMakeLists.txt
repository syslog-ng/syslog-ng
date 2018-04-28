set(CSVPARSER_SOURCES
    tags-parser.c
    tags-parser.h
    tags-parser-parser.c
    tags-parser-parser.h
    tags-parser-plugin.c
    ${CMAKE_CURRENT_BINARY_DIR}/tags-parser-grammar.h
    ${CMAKE_CURRENT_BINARY_DIR}/tags-parser-grammar.c
)

generate_y_from_ym(modules/tagsparser/tags-parser-grammar)

bison_target(CSVParserGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/tags-parser-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/tags-parser-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

add_library(tags-parser SHARED ${CSVPARSER_SOURCES})
target_include_directories(tags-parser
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(tags-parser PRIVATE syslog-ng)

install(TARGETS tags-parser LIBRARY DESTINATION lib/syslog-ng/ COMPONENT tags-parser)

add_test_subdirectory(tests)
