find_package(LibDBI)
find_package(OpenSSL)

module_switch(ENABLE_SQL "Enable SQL plugin" LIBDBI_FOUND)
if (NOT ENABLE_SQL)
  return()
endif()

set(AFSQL_SOURCES
    afsql.c
    afsql.h
    afsql-parser.c
    afsql-parser.h
    afsql-plugin.c
)

add_module(
  TARGET afsql
  GRAMMAR afsql-grammar
  INCLUDES ${LIBDBI_INCLUDE_DIRS}
  DEPENDS ${LIBDBI_LIBRARIES} OpenSSL::SSL
  SOURCES ${AFSQL_SOURCES}
)

