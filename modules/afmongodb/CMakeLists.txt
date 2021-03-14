if (ENABLE_MONGODB STREQUAL OFF)
  return()
endif()

find_package(libmongoc-1.0)

module_switch(ENABLE_MONGODB "Enable mongodb destination driver" libmongoc-1.0_FOUND)

if (ENABLE_MONGODB)

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
    INCLUDES ${MONGOC_INCLUDE_DIRS}
    DEPENDS ${MONGOC_LIBRARY}
    SOURCES ${AFMONGODB_SOURCES}
  )

  add_test_subdirectory(tests)

endif()
