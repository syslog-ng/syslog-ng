set(OTEL_PROTO_SRCDIR "${CMAKE_CURRENT_SOURCE_DIR}/opentelemetry-proto")
set(OTEL_PROTO_BUILDDIR "${CMAKE_CURRENT_BINARY_DIR}/opentelemetry-proto")
set(OTEL_PROTO_BUILDDIR ${OTEL_PROTO_BUILDDIR} PARENT_SCOPE)

set(GRPC_PROTO_SOURCES
  opentelemetry/proto/common/v1/common.proto
  opentelemetry/proto/logs/v1/logs.proto
  opentelemetry/proto/metrics/v1/metrics.proto
  opentelemetry/proto/trace/v1/trace.proto
  opentelemetry/proto/resource/v1/resource.proto)

set(GRPC_PROTO_GRPC_SOURCES
  opentelemetry/proto/collector/logs/v1/logs_service.proto
  opentelemetry/proto/collector/metrics/v1/metrics_service.proto
  opentelemetry/proto/collector/trace/v1/trace_service.proto)

protobuf_generate_cpp(
  PROTO_PATH ${OTEL_PROTO_SRCDIR}
  CPP_OUT ${OTEL_PROTO_BUILDDIR}
  OUT_SRCS GRPC_PROTO_GENERATED_SOURCES
  PROTOS ${GRPC_PROTO_SOURCES})

protobuf_generate_cpp_grpc(
  PROTO_PATH ${OTEL_PROTO_SRCDIR}
  CPP_OUT ${OTEL_PROTO_BUILDDIR}
  OUT_SRCS GRPC_PROTO_GENERATED_GRPC_SOURCES
  PROTOS ${GRPC_PROTO_GRPC_SOURCES})

add_module(
  TARGET grpc-protos
  SOURCES ${GRPC_PROTO_GENERATED_SOURCES} ${GRPC_PROTO_GENERATED_GRPC_SOURCES}
  DEPENDS ${MODULE_GRPC_LIBS}
  INCLUDES ${OTEL_PROTO_BUILDDIR}
)
