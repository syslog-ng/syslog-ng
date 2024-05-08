# Built protobuf sources should not be part of the release tarball as different
# protoc/protobuf versions can be incompatible.
#
# Always use a a different (srcdir, builddir) pair in order to avoid including
# built sources in the tarball.

GRPC_PROTOS_SRCDIR = $(top_srcdir)/modules/grpc/protos
GRPC_PROTOS_BUILDDIR = $(top_builddir)/modules/grpc/protos

GOOGLEAPIS_PROTO_SRCDIR = $(GRPC_PROTOS_SRCDIR)/googleapis-proto
GOOGLEAPIS_PROTO_BUILDDIR = $(GRPC_PROTOS_BUILDDIR)/google-proto

OPENTELEMETRY_PROTO_SRCDIR = $(GRPC_PROTOS_SRCDIR)/opentelemetry-proto
OPENTELEMETRY_PROTO_BUILDDIR = $(GRPC_PROTOS_BUILDDIR)/otlp

LOKI_PROTO_SRCDIR = $(GRPC_PROTOS_SRCDIR)/grafana-loki
LOKI_PROTO_BUILDDIR = $(GRPC_PROTOS_BUILDDIR)/loki

if ENABLE_GRPC

OPENTELEMETRY_PROTO_PROTOC_FLAGS = $(PROTOC_GEN_GRPC_CPP_PLUGIN_FLAGS) --proto_path=$(OPENTELEMETRY_PROTO_SRCDIR) --cpp_out=$(OPENTELEMETRY_PROTO_BUILDDIR)
LOKI_PROTO_PROTOC_FLAGS = $(PROTOC_GEN_GRPC_CPP_PLUGIN_FLAGS) --proto_path=$(LOKI_PROTO_SRCDIR) --cpp_out=$(LOKI_PROTO_BUILDDIR)
GOOGLEAPIS_PROTO_PROTOC_FLAGS = $(PROTOC_GEN_GRPC_CPP_PLUGIN_FLAGS) --proto_path=$(GOOGLEAPIS_PROTO_SRCDIR) --cpp_out=$(GOOGLEAPIS_PROTO_BUILDDIR)

modules/grpc/protos/otlp/%.grpc.pb.cc modules/grpc/protos/otlp/%.grpc.pb.h:
	mkdir -p $(OPENTELEMETRY_PROTO_BUILDDIR)
	$(PROTOC) $(OPENTELEMETRY_PROTO_PROTOC_FLAGS) --grpc-cpp_out=$(OPENTELEMETRY_PROTO_BUILDDIR) $*.proto

modules/grpc/protos/otlp/%.pb.cc modules/grpc/protos/otlp/%.pb.h:
	mkdir -p $(OPENTELEMETRY_PROTO_BUILDDIR)
	$(PROTOC) $(OPENTELEMETRY_PROTO_PROTOC_FLAGS) $*.proto

modules/grpc/protos/loki/%.grpc.pb.cc modules/grpc/protos/loki/%.grpc.pb.h:
	mkdir -p $(LOKI_PROTO_BUILDDIR)
	$(PROTOC) $(LOKI_PROTO_PROTOC_FLAGS) --grpc-cpp_out=$(LOKI_PROTO_BUILDDIR) $*.proto

modules/grpc/protos/loki/%.pb.cc modules/grpc/protos/loki/%.pb.h:
	mkdir -p $(LOKI_PROTO_BUILDDIR)
	$(PROTOC) $(LOKI_PROTO_PROTOC_FLAGS) $*.proto

modules/grpc/protos/google-proto/%.grpc.pb.cc modules/grpc/protos/google-proto/%.grpc.pb.h:
	mkdir -p $(GOOGLEAPIS_PROTO_BUILDDIR)
	$(PROTOC) $(GOOGLEAPIS_PROTO_PROTOC_FLAGS) --grpc-cpp_out=$(GOOGLEAPIS_PROTO_BUILDDIR) $*.proto

modules/grpc/protos/google-proto/%.pb.cc modules/grpc/protos/google-proto/%.pb.h:
	mkdir -p $(GOOGLEAPIS_PROTO_BUILDDIR)
	$(PROTOC) $(GOOGLEAPIS_PROTO_PROTOC_FLAGS) $*.proto

GRPC_PROTOS_BUILT_SOURCES = \
  $(OPENTELEMETRY_PROTO_BUILDDIR)/opentelemetry/proto/collector/logs/v1/logs_service.grpc.pb.cc \
  $(OPENTELEMETRY_PROTO_BUILDDIR)/opentelemetry/proto/collector/logs/v1/logs_service.grpc.pb.h \
  $(OPENTELEMETRY_PROTO_BUILDDIR)/opentelemetry/proto/collector/logs/v1/logs_service.pb.cc \
  $(OPENTELEMETRY_PROTO_BUILDDIR)/opentelemetry/proto/collector/logs/v1/logs_service.pb.h \
  $(OPENTELEMETRY_PROTO_BUILDDIR)/opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.cc \
  $(OPENTELEMETRY_PROTO_BUILDDIR)/opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h \
  $(OPENTELEMETRY_PROTO_BUILDDIR)/opentelemetry/proto/collector/metrics/v1/metrics_service.pb.cc \
  $(OPENTELEMETRY_PROTO_BUILDDIR)/opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h \
  $(OPENTELEMETRY_PROTO_BUILDDIR)/opentelemetry/proto/collector/trace/v1/trace_service.grpc.pb.cc \
  $(OPENTELEMETRY_PROTO_BUILDDIR)/opentelemetry/proto/collector/trace/v1/trace_service.grpc.pb.h \
  $(OPENTELEMETRY_PROTO_BUILDDIR)/opentelemetry/proto/collector/trace/v1/trace_service.pb.cc \
  $(OPENTELEMETRY_PROTO_BUILDDIR)/opentelemetry/proto/collector/trace/v1/trace_service.pb.h \
  $(OPENTELEMETRY_PROTO_BUILDDIR)/opentelemetry/proto/common/v1/common.pb.cc \
  $(OPENTELEMETRY_PROTO_BUILDDIR)/opentelemetry/proto/common/v1/common.pb.h \
  $(OPENTELEMETRY_PROTO_BUILDDIR)/opentelemetry/proto/logs/v1/logs.pb.cc \
  $(OPENTELEMETRY_PROTO_BUILDDIR)/opentelemetry/proto/logs/v1/logs.pb.h \
  $(OPENTELEMETRY_PROTO_BUILDDIR)/opentelemetry/proto/metrics/v1/metrics.pb.cc \
  $(OPENTELEMETRY_PROTO_BUILDDIR)/opentelemetry/proto/metrics/v1/metrics.pb.h \
  $(OPENTELEMETRY_PROTO_BUILDDIR)/opentelemetry/proto/resource/v1/resource.pb.cc \
  $(OPENTELEMETRY_PROTO_BUILDDIR)/opentelemetry/proto/resource/v1/resource.pb.h \
  $(OPENTELEMETRY_PROTO_BUILDDIR)/opentelemetry/proto/trace/v1/trace.pb.cc \
  $(OPENTELEMETRY_PROTO_BUILDDIR)/opentelemetry/proto/trace/v1/trace.pb.h \
  \
  $(LOKI_PROTO_BUILDDIR)/push.grpc.pb.cc \
  $(LOKI_PROTO_BUILDDIR)/push.grpc.pb.h \
  $(LOKI_PROTO_BUILDDIR)/push.pb.cc \
  $(LOKI_PROTO_BUILDDIR)/push.pb.h \
  \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/api/annotations.pb.cc \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/api/annotations.pb.h \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/api/client.pb.cc \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/api/client.pb.h \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/api/field_behavior.pb.cc \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/api/field_behavior.pb.h \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/api/http.pb.cc \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/api/http.pb.h \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/api/launch_stage.pb.cc \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/api/launch_stage.pb.h \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/api/resource.pb.cc \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/api/resource.pb.h \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/cloud/bigquery/storage/v1/annotations.pb.cc \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/cloud/bigquery/storage/v1/annotations.pb.h \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/cloud/bigquery/storage/v1/arrow.pb.cc \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/cloud/bigquery/storage/v1/arrow.pb.h \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/cloud/bigquery/storage/v1/avro.pb.cc \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/cloud/bigquery/storage/v1/avro.pb.h \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/cloud/bigquery/storage/v1/protobuf.pb.cc \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/cloud/bigquery/storage/v1/protobuf.pb.h \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/cloud/bigquery/storage/v1/storage.grpc.pb.cc \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/cloud/bigquery/storage/v1/storage.grpc.pb.h \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/cloud/bigquery/storage/v1/storage.pb.cc \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/cloud/bigquery/storage/v1/storage.pb.h \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/cloud/bigquery/storage/v1/stream.pb.cc \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/cloud/bigquery/storage/v1/stream.pb.h \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/cloud/bigquery/storage/v1/table.pb.cc \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/cloud/bigquery/storage/v1/table.pb.h \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/rpc/status.pb.cc \
  $(GOOGLEAPIS_PROTO_BUILDDIR)/google/rpc/status.pb.h

lib_LTLIBRARIES += modules/grpc/protos/libgrpc-protos.la

nodist_modules_grpc_protos_libgrpc_protos_la_SOURCES = $(GRPC_PROTOS_BUILT_SOURCES)
modules_grpc_protos_libgrpc_protos_la_SOURCES = \
  modules/grpc/protos/apphook.cpp \
  modules/grpc/protos/apphook.h

modules_grpc_protos_libgrpc_protos_la_CXXFLAGS = \
  $(AM_CXXFLAGS) \
  $(PROTOBUF_CFLAGS) \
  $(GRPCPP_CFLAGS) \
  -I$(GOOGLEAPIS_PROTO_SRCDIR) \
  -I$(GOOGLEAPIS_PROTO_BUILDDIR) \
  -I$(OPENTELEMETRY_PROTO_SRCDIR) \
  -I$(OPENTELEMETRY_PROTO_BUILDDIR) \
  -I$(LOKI_PROTO_SRCDIR) \
  -I$(LOKI_PROTO_BUILDDIR)

if ENABLE_EXTRA_WARNINGS
modules_grpc_protos_libgrpc_protos_la_CXXFLAGS += -Wno-switch-default
endif

modules_grpc_protos_libgrpc_protos_la_LIBADD = $(MODULE_DEPS_LIBS) $(PROTOBUF_LIBS) $(GRPCPP_LIBS)
nodist_EXTRA_modules_grpc_protos_libgrpc_protos_la_SOURCES = force-cpp-linker-with-default-stdlib.cpp
EXTRA_modules_grpc_protos_libgrpc_protos_la_DEPENDENCIES = $(MODULE_DEPS_LIBS)

CLEANFILES += \
  $(GRPC_PROTOS_BUILT_SOURCES)

BUILT_SOURCES += \
  $(GRPC_PROTOS_BUILT_SOURCES)

NODIST_BUILT_SOURCES += \
  $(GRPC_PROTOS_BUILT_SOURCES)

endif

EXTRA_DIST += \
  $(OPENTELEMETRY_PROTO_SRCDIR) \
  $(LOKI_PROTO_SRCDIR) \
  $(GOOGLEAPIS_PROTO_SRCDIR)
