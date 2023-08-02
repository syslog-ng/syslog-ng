set(OTEL_CPP_SOURCES
  otel-source.cpp
  otel-source.hpp
  otel-source.h
  otel-source-services.hpp
  otel-servicecall.hpp
  otel-protobuf-parser.hpp
  otel-protobuf-parser.cpp
  otel-protobuf-parser.h
  grpc-credentials-builder.cpp
  grpc-credentials-builder.hpp
  grpc-credentials-builder.h
  otel-protobuf-formatter.hpp
  otel-protobuf-formatter.cpp
  otel-dest.hpp
  otel-dest.cpp
  otel-dest.h
  otel-dest-worker.hpp
  otel-dest-worker.cpp)

set(OTEL_SOURCES
  otel-plugin.c
  otel-parser.c
  otel-parser.h
)

add_module(
  TARGET otel-cpp
  SOURCES ${OTEL_CPP_SOURCES}
  DEPENDS ${MODULE_GRPC_LIBS} grpc-protos
  INCLUDES ${OTEL_PROTO_BUILDDIR}
  LIBRARY_TYPE STATIC
)

add_module(
  TARGET otel
  GRAMMAR otel-grammar
  DEPENDS otel-cpp
  SOURCES ${OTEL_SOURCES}
)

add_test_subdirectory(tests)
