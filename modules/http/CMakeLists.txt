if (NOT DEFINED ENABLE_CURL OR ENABLE_CURL)
  find_package(Curl)
endif()

module_switch(ENABLE_CURL "Enable http destination" Curl_FOUND)
if (NOT ENABLE_CURL)
  return ()
endif ()

if (NOT Curl_FOUND)
  message(FATAL_ERROR "HTTP module enabled, but libcurl not found")
endif ()

find_package(ZLIB)
if(ZLIB_FOUND)
    add_compile_definitions(SYSLOG_NG_HAVE_ZLIB)
endif()

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
    autodetect-ca-location.h
    autodetect-ca-location.c
    compression.h
    compression.c
)

set(HTTP_MODULE_DEV_HEADERS
  http-signals.h
)

add_module(
  TARGET http
  GRAMMAR http-grammar
  INCLUDES ${Curl_INCLUDE_DIR}
           ${ZLIB_INCLUDE_DIRS}
  DEPENDS ${Curl_LIBRARIES}
          ${ZLIB_LIBRARIES}
  SOURCES ${HTTP_DESTINATION_SOURCES}
)

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
curl_detect_compile_option(CURLOPT_TLS13_CIPHERS)
curl_detect_compile_option(CURLOPT_SSL_VERIFYSTATUS)
curl_detect_compile_option(CURLOPT_REDIR_PROTOCOLS_STR)

# Full URL parsing support
curl_detect_compile_option(curl_url)
curl_detect_compile_option(CURLU_ALLOW_SPACE)
curl_detect_compile_option(CURLUE_BAD_SCHEME)
curl_detect_compile_option(CURLUE_BAD_HOSTNAME)
curl_detect_compile_option(CURLUE_BAD_PORT_NUMBER)
curl_detect_compile_option(CURLUE_BAD_USER)
curl_detect_compile_option(CURLUE_BAD_PASSWORD)
curl_detect_compile_option(CURLUE_MALFORMED_INPUT)
curl_detect_compile_option(CURLUE_LAST)
curl_detect_compile_option(CURLUPART_SCHEME)
curl_detect_compile_option(CURLUPART_HOST)
curl_detect_compile_option(CURLUPART_PORT)
curl_detect_compile_option(CURLUPART_USER)
curl_detect_compile_option(CURLUPART_PASSWORD)
curl_detect_compile_option(CURLUPART_URL)

install(FILES ${HTTP_MODULE_DEV_HEADERS} DESTINATION include/syslog-ng/modules/http/)

add_test_subdirectory(tests)
