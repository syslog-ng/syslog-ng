set(OTEL_PROTO_SRCDIR "${CMAKE_CURRENT_SOURCE_DIR}/opentelemetry-proto")
set(OTEL_PROTO_BUILDDIR "${CMAKE_CURRENT_BINARY_DIR}/proto")

set(OTEL_PROTO_SOURCES
  opentelemetry/proto/common/v1/common.proto
  opentelemetry/proto/logs/v1/logs.proto
  opentelemetry/proto/metrics/v1/metrics.proto
  opentelemetry/proto/trace/v1/trace.proto
  opentelemetry/proto/resource/v1/resource.proto)

set(OTEL_PROTO_GRPC_SOURCES
  opentelemetry/proto/collector/logs/v1/logs_service.proto
  opentelemetry/proto/collector/metrics/v1/metrics_service.proto
  opentelemetry/proto/collector/trace/v1/trace_service.proto)

protobuf_generate_cpp(
  PROTO_PATH ${OTEL_PROTO_SRCDIR}
  CPP_OUT ${OTEL_PROTO_BUILDDIR}
  OUT_SRCS OTEL_PROTO_GENERATED_SOURCES
  PROTOS ${OTEL_PROTO_SOURCES})

protobuf_generate_cpp_grpc(
  PROTO_PATH ${OTEL_PROTO_SRCDIR}
  CPP_OUT ${OTEL_PROTO_BUILDDIR}
  OUT_SRCS OTEL_PROTO_GENERATED_GRPC_SOURCES
  PROTOS ${OTEL_PROTO_GRPC_SOURCES})

set(OTEL_CPP_SOURCES
  otel-source.cpp
  otel-source.hpp
  otel-source.h
  otel-source-services.hpp
  otel-servicecall.hpp
  otel-protobuf-parser.hpp
  otel-protobuf-parser.cpp
  ${OTEL_PROTO_GENERATED_SOURCES}
  ${OTEL_PROTO_GENERATED_GRPC_SOURCES})

set(OTEL_SOURCES
  otel-plugin.c
  otel-parser.c
  otel-parser.h
)

add_module(
  TARGET otel-cpp
  SOURCES ${OTEL_CPP_SOURCES}
  DEPENDS ${MODULE_GRPC_LIBS}
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
