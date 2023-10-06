if(ENABLE_CLOUD_AUTH AND NOT ENABLE_CPP)
  message(FATAL_ERROR "C++ support is mandatory when the Cloud Auth module is enabled.")
endif()

module_switch(ENABLE_CLOUD_AUTH "Enable Cloud Auth" ENABLE_CPP)

if(NOT ENABLE_CLOUD_AUTH OR NOT ENABLE_CPP)
  return()
endif()

set(CLOUD_AUTH_CPP_SOURCES
  cloud-auth.c
  cloud-auth.h
  cloud-auth.cpp
  cloud-auth.hpp
  google-auth.h
  google-auth.cpp
  google-auth.hpp
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
  LIBRARY_TYPE STATIC
)

add_module(
  TARGET cloud-auth
  GRAMMAR cloud-auth-grammar
  DEPENDS cloud-auth-cpp
  INCLUDES ${PROJECT_SOURCE_DIR}
           ${PROJECT_SOURCE_DIR}/modules/cloud-auth
  SOURCES ${OTEL_SOURCES}
)
