module_switch(ENABLE_STARDATE "Enable stardate")
if (NOT ENABLE_STARDATE)
  return()
endif()

set(stardate_SOURCES
    stardate-plugin.c)

add_module(
  TARGET stardate
  DEPENDS m
  SOURCES ${stardate_SOURCES}
)

add_test_subdirectory(tests)
