find_package(Curl)

module_switch(ENABLE_CURL "Enable http destination" Curl_FOUND)
if (NOT ENABLE_CURL)
  return ()
endif ()

if (NOT Curl_FOUND)
  message(FATAL_ERROR "HTTP module enabled, but libcurl not found")
endif ()

set(HTTP_DESTINATION_SOURCES
    http.h
    http.c
    http-worker.h
    http-worker.c
    http-loadbalancer.h
    http-loadbalancer.c
    http-curl-header-list.h
    http-curl-header-list.c
    http-parser.c
    http-parser.h
    http-plugin.c
    response-handler.h
    response-handler.c
    http-signals.h
    ${CMAKE_CURRENT_BINARY_DIR}/http-grammar.c
    ${CMAKE_CURRENT_BINARY_DIR}/http-grammar.h
)

set(HTTP_MODULE_DEV_HEADERS
  http-signals.h
)

generate_y_from_ym(modules/http/http-grammar)

bison_target(HttpParserGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/http-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/http-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

add_library(http SHARED ${HTTP_DESTINATION_SOURCES})

function (curl_detect_compile_option NAME)
  set(CMAKE_REQUIRED_INCLUDES "${Curl_INCLUDE_DIR}")
  set(CMAKE_EXTRA_INCLUDE_FILES "curl/curl.h")
  string(TOUPPER ${NAME} NAME_CAPITAL)

  check_type_size(${NAME} SYSLOG_NG_HAVE_DECL_${NAME_CAPITAL})
  if (SYSLOG_NG_HAVE_DECL_${NAME_CAPITAL})
    target_compile_definitions(http PRIVATE "-DSYSLOG_NG_HAVE_DECL_${NAME_CAPITAL}=1")
  else()
    target_compile_definitions(http PRIVATE "-DSYSLOG_NG_HAVE_DECL_${NAME_CAPITAL}=0")
  endif()
endfunction ()

curl_detect_compile_option(CURL_SSLVERSION_TLSv1_0)
curl_detect_compile_option(CURL_SSLVERSION_TLSv1_1)
curl_detect_compile_option(CURL_SSLVERSION_TLSv1_2)
curl_detect_compile_option(CURL_SSLVERSION_TLSv1_3)

target_include_directories (http PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories (http PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories (http PRIVATE ${Curl_INCLUDE_DIR})
target_link_libraries(http PRIVATE syslog-ng ${Curl_LIBRARIES})

install(TARGETS http LIBRARY DESTINATION lib/syslog-ng/)
install(FILES ${HTTP_MODULE_DEV_HEADERS} DESTINATION include/syslog-ng/modules/http/)

add_test_subdirectory(tests)
