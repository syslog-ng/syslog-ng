find_package(Curl)

if (Curl_FOUND)
  option(ENABLE_CURL "Enable http destination" ON)
else ()
  option(ENABLE_CURL "Enable http destination" OFF)
endif()

if (NOT ENABLE_CURL)
  return ()
endif ()

if (NOT Curl_FOUND)
  message(FATAL_ERROR "HTTP module enabled, but libcurl not found")
endif ()

set(HTTP_DESTINATION_SOURCES
    http.h
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

add_library(http SHARED ${HTTP_DESTINATION_SOURCES})

target_include_directories (http PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories (http PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories (http PRIVATE ${Curl_INCLUDE_DIR})
target_link_libraries(http PRIVATE syslog-ng ${Curl_LIBRARIES})

install(TARGETS http LIBRARY DESTINATION lib/syslog-ng/)

add_test_subdirectory(tests)
