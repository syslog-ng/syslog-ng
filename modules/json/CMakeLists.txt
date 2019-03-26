set(JSON_SOURCES
    format-json.c
    format-json.h
    json-parser.c
    json-parser.h
    json-parser-parser.c
    json-parser-parser.h
    dot-notation.c
    dot-notation.h
    json-plugin.c
    ${CMAKE_CURRENT_BINARY_DIR}/json-parser-grammar.h
    ${CMAKE_CURRENT_BINARY_DIR}/json-parser-grammar.c
)

set(JSON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

external_or_find_package(JSONC)

module_switch(ENABLE_JSON "Enable JSON plugin" JSONC_FOUND)
if (ENABLE_JSON)
    generate_y_from_ym(modules/json/json-parser-grammar)

    bison_target(JsonGrammar
        ${CMAKE_CURRENT_BINARY_DIR}/json-parser-grammar.y
        ${CMAKE_CURRENT_BINARY_DIR}/json-parser-grammar.c
        COMPILE_FLAGS ${BISON_FLAGS})

    add_library(json-plugin SHARED ${JSON_SOURCES})
    target_link_libraries (json-plugin PUBLIC ${JSONC_LIBRARY})
    target_include_directories (json-plugin SYSTEM PRIVATE ${JSONC_INCLUDE_DIR})
    target_include_directories (json-plugin PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(json-plugin PRIVATE syslog-ng)

if (JSONC_INTERNAL)
    add_dependencies(json-plugin JSONC)
endif()

    install(TARGETS json-plugin LIBRARY DESTINATION lib/syslog-ng/ COMPONENT json)
    add_test_subdirectory(tests)
endif()
