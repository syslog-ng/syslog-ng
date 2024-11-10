set(OTEL_PROTO_SRCDIR "${CMAKE_CURRENT_SOURCE_DIR}/opentelemetry-proto")
set(OTEL_PROTO_BUILDDIR "${CMAKE_CURRENT_BINARY_DIR}/opentelemetry-proto")
set(OTEL_PROTO_BUILDDIR ${OTEL_PROTO_BUILDDIR} PARENT_SCOPE)

set(LOKI_PROTO_SRCDIR "${CMAKE_CURRENT_SOURCE_DIR}/grafana-loki")
set(LOKI_PROTO_BUILDDIR "${CMAKE_CURRENT_BINARY_DIR}/grafana-loki")
set(LOKI_PROTO_BUILDDIR ${LOKI_PROTO_BUILDDIR} PARENT_SCOPE)

set(GOOGLEAPIS_PROTO_SRCDIR "${CMAKE_CURRENT_SOURCE_DIR}/googleapis-proto")
set(GOOGLEAPIS_PROTO_BUILDDIR "${CMAKE_CURRENT_BINARY_DIR}/googleapis-proto")
set(GOOGLEAPIS_PROTO_BUILDDIR ${GOOGLEAPIS_PROTO_BUILDDIR} PARENT_SCOPE)

set(CLICKHOUSE_PROTO_SRCDIR "${CMAKE_CURRENT_SOURCE_DIR}/clickhouse-proto")
set(CLICKHOUSE_PROTO_BUILDDIR "${CMAKE_CURRENT_BINARY_DIR}/clickhouse-proto")
set(CLICKHOUSE_PROTO_BUILDDIR ${CLICKHOUSE_PROTO_BUILDDIR} PARENT_SCOPE)

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

set(GOOGLEAPIS_PROTO_SOURCES
  google/api/annotations.proto
  google/api/client.proto
  google/api/field_behavior.proto
  google/api/http.proto
  google/api/launch_stage.proto
  google/api/resource.proto
  google/cloud/bigquery/storage/v1/annotations.proto
  google/cloud/bigquery/storage/v1/arrow.proto
  google/cloud/bigquery/storage/v1/avro.proto
  google/cloud/bigquery/storage/v1/protobuf.proto
  google/cloud/bigquery/storage/v1/stream.proto
  google/cloud/bigquery/storage/v1/table.proto
  google/rpc/status.proto
)

set(GOOGLEAPIS_PROTO_GRPC_SOURCES
  google/cloud/bigquery/storage/v1/storage.proto
)

set(LOKI_PROTO_GRPC_SOURCES
  push.proto)

set(CLICKHOUSE_PROTO_GRPC_SOURCES
  clickhouse_grpc.proto
)

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

protobuf_generate_cpp_grpc(
  PROTO_PATH ${LOKI_PROTO_SRCDIR}
  CPP_OUT ${LOKI_PROTO_BUILDDIR}
  OUT_SRCS LOKI_PROTO_GENERATED_GRPC_SOURCES
  PROTOS ${LOKI_PROTO_GRPC_SOURCES})

protobuf_generate_cpp(
  PROTO_PATH ${GOOGLEAPIS_PROTO_SRCDIR}
  CPP_OUT ${GOOGLEAPIS_PROTO_BUILDDIR}
  OUT_SRCS GOOGLEAPIS_PROTO_GENERATED_SOURCES
  PROTOS ${GOOGLEAPIS_PROTO_SOURCES})

protobuf_generate_cpp_grpc(
  PROTO_PATH ${GOOGLEAPIS_PROTO_SRCDIR}
  CPP_OUT ${GOOGLEAPIS_PROTO_BUILDDIR}
  OUT_SRCS GOOGLEAPIS_PROTO_GENERATED_GRPC_SOURCES
  PROTOS ${GOOGLEAPIS_PROTO_GRPC_SOURCES})

protobuf_generate_cpp(
  PROTO_PATH ${CLICKHOUSE_PROTO_SRCDIR}
  CPP_OUT ${CLICKHOUSE_PROTO_BUILDDIR}
  OUT_SRCS CLICKHOUSE_PROTO_GENERATED_SOURCES
  PROTOS ${CLICKHOUSE_PROTO_SOURCES})

protobuf_generate_cpp_grpc(
  PROTO_PATH ${CLICKHOUSE_PROTO_SRCDIR}
  CPP_OUT ${CLICKHOUSE_PROTO_BUILDDIR}
  OUT_SRCS CLICKHOUSE_PROTO_GENERATED_GRPC_SOURCES
  PROTOS ${CLICKHOUSE_PROTO_GRPC_SOURCES})

add_library(grpc-protos SHARED
  ${OTEL_PROTO_GENERATED_SOURCES}
  ${OTEL_PROTO_GENERATED_GRPC_SOURCES}
  ${LOKI_PROTO_GENERATED_GRPC_SOURCES}
  ${GOOGLEAPIS_PROTO_GENERATED_SOURCES}
  ${GOOGLEAPIS_PROTO_GENERATED_GRPC_SOURCES}
  ${CLICKHOUSE_PROTO_GENERATED_SOURCES}
  ${CLICKHOUSE_PROTO_GENERATED_GRPC_SOURCES}
  apphook.cpp
)

target_link_libraries(grpc-protos PRIVATE ${MODULE_GRPC_LIBS} syslog-ng)
target_include_directories(grpc-protos
  PUBLIC ${OTEL_PROTO_BUILDDIR} ${LOKI_PROTO_BUILDDIR} ${GOOGLEAPIS_PROTO_BUILDDIR} ${CLICKHOUSE_PROTO_BUILDDIR}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/modules/grpc
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${PROJECT_SOURCE_DIR}
)

target_compile_options(grpc-protos PRIVATE -Wno-switch-default -Wno-double-promotion -Wno-deprecated -DPROTOBUF_ENABLE_DEBUG_LOGGING_MAY_LEAK_PII=0)
install(TARGETS grpc-protos LIBRARY DESTINATION lib/syslog-ng COMPONENT grpc-protos)
