if(ENABLE_CLOUD_AUTH AND NOT ENABLE_CPP)
  message(FATAL_ERROR "C++ support is mandatory when the Cloud Auth module is enabled.")
endif()

if(ENABLE_CLOUD_AUTH AND NOT ENABLE_CURL)
  message(FATAL_ERROR "cURL is mandatory when the Cloud Auth module is enabled.")
endif()

module_switch(ENABLE_CLOUD_AUTH "Enable Cloud Auth" ENABLE_CPP ENABLE_CURL)

if(NOT ENABLE_CLOUD_AUTH)
  return()
endif()

find_package(Curl)

if (NOT Curl_FOUND)
  message(FATAL_ERROR "cURL support for cloud-auth() enabled, but libcurl not found")
endif ()

set(CLOUD_AUTH_CPP_SOURCES
  cloud-auth.c
  cloud-auth.h
  cloud-auth.cpp
  cloud-auth.hpp
  google-auth.h
  google-auth.cpp
  google-auth.hpp
  azure-auth.h
  azure-auth.cpp
  azure-auth.hpp
)

set(OTEL_SOURCES
  cloud-auth-plugin.c
  cloud-auth-parser.c
  cloud-auth-parser.h
)

add_module(
  TARGET cloud-auth-cpp
  SOURCES ${CLOUD_AUTH_CPP_SOURCES}
  INCLUDES ${PROJECT_SOURCE_DIR}
           ${PROJECT_SOURCE_DIR}/modules/cloud-auth
           ${PROJECT_SOURCE_DIR}/modules/cloud-auth/jwt-cpp/include
           ${Curl_INCLUDE_DIR}
  DEPENDS ${Curl_LIBRARIES}
  LIBRARY_TYPE STATIC
)

add_module(
  TARGET cloud_auth
  GRAMMAR cloud-auth-grammar
  DEPENDS cloud-auth-cpp
  INCLUDES ${PROJECT_SOURCE_DIR}
           ${PROJECT_SOURCE_DIR}/modules/cloud-auth
  SOURCES ${OTEL_SOURCES}
)
