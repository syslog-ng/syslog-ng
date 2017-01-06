option (ENABLE_CURL "Enable curl destination" ON)

set(HTTP_PARSER_SOURCES
    http-plugin.h
    http.c
    http-parser.c
    http-parser.h
    http-plugin.c
    ${CMAKE_CURRENT_BINARY_DIR}/http-grammar.c
    ${CMAKE_CURRENT_BINARY_DIR}/http-grammar.h
)

generate_y_from_ym(modules/http/http-grammar)

bison_target(HttpParserGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/http-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/http-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

find_package(Curl REQUIRED)

if (Curl_FOUND)
    option(ENABLE_CURL "Enable curl destination" ON)
else()
    option(ENABLE_CURL "Enable curl destination" OFF)
endif()

if (ENABLE_CURL)
    add_library(curl MODULE ${HTTP_PARSER_SOURCES})

    target_include_directories (curl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_include_directories (curl PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_include_directories (curl PRIVATE ${Curl_INCLUDE_DIR})
    target_link_libraries(curl PRIVATE syslog-ng ${Curl_LIBRARIES})

    install(TARGETS curl LIBRARY DESTINATION lib/syslog-ng/)
endif()
