find_package(libmongoc-1.0)

module_switch(ENABLE_MONGODB "Enable mongodb destination driver" libmongoc-1.0_FOUND)

if (libmongoc-1.0_FOUND)

  set(AFMONGODB_HEADERS
    "afmongodb.h"
    "afmongodb-parser.h"
    "afmongodb-private.h"
    "${CMAKE_CURRENT_BINARY_DIR}/afmongodb-grammar.h"
  )

  set(AFMONGODB_SOURCES
    "afmongodb.c"
    "afmongodb-parser.c"
    "${CMAKE_CURRENT_BINARY_DIR}/afmongodb-grammar.c"
  )

set(AFMONGODB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

generate_y_from_ym(modules/afmongodb/afmongodb-grammar)
bison_target(AFMongoDBGrammar
  ${CMAKE_CURRENT_BINARY_DIR}/afmongodb-grammar.y
  ${CMAKE_CURRENT_BINARY_DIR}/afmongodb-grammar.c
  COMPILE_FLAGS ${BISON_FLAGS})

include_directories (afmongodb SYSTEM PRIVATE ${MONGOC_INCLUDE_DIRS})
add_library(afmongodb SHARED ${AFMONGODB_SOURCES})
target_include_directories (afmongodb PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(afmongodb PRIVATE syslog-ng ${MONGOC_LIBRARY})

install(TARGETS afmongodb LIBRARY DESTINATION lib/syslog-ng/ COMPONENT afmongodb)

add_test_subdirectory(tests)

endif()
