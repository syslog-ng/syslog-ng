# Built protobuf sources should not be part of the release tarball as different
# protoc/protobuf versions can be incompatible.
#
# Always use a a different (srcdir, builddir) pair in order to avoid including
# built sources in the tarball.

GRPC_PROTOS_SRCDIR = $(top_srcdir)/modules/grpc/protos
GRPC_PROTOS_BUILDDIR = $(top_builddir)/modules/grpc/protos

OPENTELEMETRY_PROTO_SRCDIR = $(GRPC_PROTOS_SRCDIR)/opentelemetry-proto
OPENTELEMETRY_PROTO_BUILDDIR = $(GRPC_PROTOS_BUILDDIR)/otlp
OPENTELEMETRY_PROTO_PROTOC_FLAGS = $(PROTOC_GEN_GRPC_CPP_PLUGIN_FLAGS) --proto_path=$(OPENTELEMETRY_PROTO_SRCDIR) --cpp_out=$(OPENTELEMETRY_PROTO_BUILDDIR)

LOKI_PROTO_SRCDIR = $(GRPC_PROTOS_SRCDIR)/grafana-loki
LOKI_PROTO_BUILDDIR = $(GRPC_PROTOS_BUILDDIR)/loki
LOKI_PROTO_PROTOC_FLAGS = $(PROTOC_GEN_GRPC_CPP_PLUGIN_FLAGS) --proto_path=$(LOKI_PROTO_SRCDIR) --cpp_out=$(LOKI_PROTO_BUILDDIR)

if ENABLE_GRPC

GRPC_PROTOS_BUILT_SOURCES =

if BUILD_OTEL # means ENABLE_GRPC && ENABLE_OTEL

modules/grpc/protos/otlp/%.grpc.pb.cc modules/grpc/protos/otlp/%.grpc.pb.h:
	mkdir -p $(OPENTELEMETRY_PROTO_BUILDDIR)
	$(PROTOC) $(OPENTELEMETRY_PROTO_PROTOC_FLAGS) --grpc-cpp_out=$(OPENTELEMETRY_PROTO_BUILDDIR) $*.proto

modules/grpc/protos/otlp/%.pb.cc modules/grpc/protos/otlp/%.pb.h:
	mkdir -p $(OPENTELEMETRY_PROTO_BUILDDIR)
	$(PROTOC) $(OPENTELEMETRY_PROTO_PROTOC_FLAGS) $*.proto

GRPC_PROTOS_BUILT_SOURCES += \
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
  $(OPENTELEMETRY_PROTO_BUILDDIR)/opentelemetry/proto/trace/v1/trace.pb.h

endif # if BUILD_OTEL

if BUILD_LOKI # means ENABLE_GRPC && ENABLE_LOKI

modules/grpc/protos/loki/%.grpc.pb.cc modules/grpc/protos/loki/%.grpc.pb.h:
	mkdir -p $(LOKI_PROTO_BUILDDIR)
	$(PROTOC) $(LOKI_PROTO_PROTOC_FLAGS) --grpc-cpp_out=$(LOKI_PROTO_BUILDDIR) $*.proto

modules/grpc/protos/loki/%.pb.cc modules/grpc/protos/loki/%.pb.h:
	mkdir -p $(LOKI_PROTO_BUILDDIR)
	$(PROTOC) $(LOKI_PROTO_PROTOC_FLAGS) $*.proto

GRPC_PROTOS_BUILT_SOURCES += \
  $(LOKI_PROTO_BUILDDIR)/push.grpc.pb.cc \
  $(LOKI_PROTO_BUILDDIR)/push.grpc.pb.h \
  $(LOKI_PROTO_BUILDDIR)/push.pb.cc \
  $(LOKI_PROTO_BUILDDIR)/push.pb.h

endif # if BUILD_LOKI

lib_LTLIBRARIES += modules/grpc/protos/libgrpc-protos.la

nodist_modules_grpc_protos_libgrpc_protos_la_SOURCES = $(GRPC_PROTOS_BUILT_SOURCES)

modules_grpc_protos_libgrpc_protos_la_CXXFLAGS = \
  $(AM_CXXFLAGS) \
  $(PROTOBUF_CLFAGS) \
  $(GRPCPP_CFLAGS) \
  -I$(OPENTELEMETRY_PROTO_SRCDIR) \
  -I$(OPENTELEMETRY_PROTO_BUILDDIR) \
  -I$(LOKI_PROTO_SRCDIR) \
  -I$(LOKI_PROTO_BUILDDIR)

if ENABLE_EXTRA_WARNINGS
modules_grpc_protos_libgrpc_protos_la_CXXFLAGS += -Wno-switch-default
endif

modules_grpc_protos_libgrpc_protos_la_LIBADD = $(MODULE_DEPS_LIBS) $(PROTOBUF_LIBS) $(GRPCPP_LIBS) -lstdc++
modules_grpc_protos_libgrpc_protos_la_DEPENDENCIES = $(MODULE_DEPS_LIBS)

CLEANFILES += \
  $(GRPC_PROTOS_BUILT_SOURCES)

BUILT_SOURCES += \
  $(GRPC_PROTOS_BUILT_SOURCES)

NODIST_BUILT_SOURCES += \
  $(GRPC_PROTOS_BUILT_SOURCES)

endif # if ENABLE_GRPC

EXTRA_DIST += \
  $(OPENTELEMETRY_PROTO_SRCDIR) \
  $(LOKI_PROTO_SRCDIR)
