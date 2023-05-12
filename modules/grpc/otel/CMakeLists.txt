set(OTEL_CPP_SOURCES
  otel-source.cpp
  otel-source.hpp
  otel-source.h
)

set(OTEL_SOURCES
  otel-plugin.c
  otel-parser.c
  otel-parser.h
)

add_module(
  TARGET otel-cpp
  SOURCES ${OTEL_CPP_SOURCES}
  LIBRARY_TYPE STATIC
)

add_module(
  TARGET otel
  GRAMMAR otel-grammar
  DEPENDS otel-cpp
  SOURCES ${OTEL_SOURCES}
)
