set (GRAPHITE_SOURCES
    graphite-plugin.c
    graphite-output.h
    graphite-output.c
)

add_module(
  TARGET graphite
  SOURCES ${GRAPHITE_SOURCES}
)

add_test_subdirectory(tests)
