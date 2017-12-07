
set(HTTP_DESTINATION_SOURCES
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

find_package(Curl)

if (Curl_FOUND)
    option(ENABLE_CURL "Enable http destination" ON)
else()
    option(ENABLE_CURL "Enable http destination" OFF)
endif()

if (ENABLE_CURL)
    add_library(http MODULE ${HTTP_DESTINATION_SOURCES})

    target_include_directories (http PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_include_directories (http PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_include_directories (http PRIVATE ${Curl_INCLUDE_DIR})
    target_link_libraries(http PRIVATE syslog-ng ${Curl_LIBRARIES})

    install(TARGETS http LIBRARY DESTINATION lib/syslog-ng/)
endif()
