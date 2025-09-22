if(NOT DEFINED ENABLE_MONGODB OR ENABLE_MONGODB)
  set(LMC_1_MIN_VERSION 1.0.0)
  set(LMC_MIN_VERSION 2.0.0)

  find_package(PkgConfig REQUIRED)

  # First try libmongoc-1.0
  pkg_check_modules(LIBMONGO libmongoc-1.0>=${LMC_1_MIN_VERSION})

  if(LIBMONGO_FOUND)
    set(ENABLE_MONGODB_INFO_STR "v1" CACHE INTERNAL "MongoDB version")
    set(MONGOC_HEADER "<mongoc.h>")

    # libbson-1.0 is needed for libmongoc-1.0
    pkg_check_modules(LIBBSON libbson-1.0>=${LMC_1_MIN_VERSION})

    if(NOT LIBBSON_FOUND)
      message(FATAL_ERROR "libbson-1.0 is required for libmongoc-1.0")
    endif()

    include_directories(${LIBMONGO_INCLUDE_DIRS} ${LIBBSON_INCLUDE_DIRS})
    link_directories(${LIBMONGO_LIBRARY_DIRS} ${LIBBSON_LIBRARY_DIRS})

  else()
    # libmongoc-1.0 not found, try mongoc2
    pkg_check_modules(LIBMONGO mongoc2>=${LMC_MIN_VERSION})

    if(LIBMONGO_FOUND)
      set(SYSLOG_NG_HAVE_MONGOC_V2 1 CACHE INTERNAL "MongoDB version 2 is available")
      set(ENABLE_MONGODB_INFO_STR "v2" CACHE INTERNAL "MongoDB version")
      set(MONGOC_HEADER "<mongoc/mongoc.h>")

      include_directories(${LIBMONGO_INCLUDE_DIRS})
      link_directories(${LIBMONGO_LIBRARY_DIRS})
    endif()
  endif()
endif()

if(ENABLE_MONGODB AND NOT LIBMONGO_FOUND)
  message(FATAL_ERROR "Could not find suitable mongo-c-driver (or its dependencies), and MongoDB support was explicitly enabled.")
endif()

module_switch(ENABLE_MONGODB "Enable mongodb destination driver" LIBMONGO_FOUND)

if(NOT ENABLE_MONGODB)
  return()
endif()

include(CheckCSourceCompiles)
set(CMAKE_REQUIRED_INCLUDES ${LIBMONGO_INCLUDE_DIRS})

if(ENABLE_MONGODB_INFO_STR STREQUAL "v1")
  list(APPEND CMAKE_REQUIRED_INCLUDES ${LIBBSON_INCLUDE_DIRS})
endif()

check_c_source_compiles("
#include ${MONGOC_HEADER}
#include <bson/bson.h>
int main() {
    (void) mongoc_uri_set_option_as_int32;
    return 0;
}" HAVE_MONGOC_URI_SET_OPTION_AS_INT32)
check_c_source_compiles("
#include ${MONGOC_HEADER}
int main() {
    (void) MONGOC_URI_SERVERSELECTIONTIMEOUTMS;
    return 0;
}" HAVE_MONGOC_URI_SERVERSELECTIONTIMEOUTMS)

if(HAVE_MONGOC_URI_SET_OPTION_AS_INT32)
  set(SYSLOG_NG_HAVE_DECL_MONGOC_URI_SET_OPTION_AS_INT32 1 CACHE INTERNAL "")
endif()

if(HAVE_MONGOC_URI_SERVERSELECTIONTIMEOUTMS)
  set(SYSLOG_NG_HAVE_DECL_MONGOC_URI_SERVERSELECTIONTIMEOUTMS 1 CACHE INTERNAL "")
endif()

set(AFMONGODB_SOURCES
  "afmongodb.h"
  "afmongodb.c"
  "afmongodb-worker.h"
  "afmongodb-worker.c"
  "afmongodb-parser.h"
  "afmongodb-parser.c"
  "afmongodb-private.h"
)

add_module(
  TARGET afmongodb
  GRAMMAR afmongodb-grammar
  DEPENDS ${LIBMONGO_LDFLAGS}
  SOURCES ${AFMONGODB_SOURCES}
)

add_test_subdirectory(tests)
