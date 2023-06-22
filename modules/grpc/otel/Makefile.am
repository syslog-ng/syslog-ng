OTEL_PROTO_SRCDIR = $(top_srcdir)/modules/grpc/otel/opentelemetry-proto
OTEL_PROTO_BUILDDIR = $(top_builddir)/modules/grpc/otel/proto

if ENABLE_GRPC

OTEL_PROTO_PROTOC_FLAGS = $(PROTOC_GEN_GRPC_CPP_PLUGIN_FLAGS) --proto_path=$(OTEL_PROTO_SRCDIR) --cpp_out=$(OTEL_PROTO_BUILDDIR)

modules/grpc/otel/proto/%.grpc.pb.cc modules/grpc/otel/proto/%.grpc.pb.h:
	mkdir -p $(OTEL_PROTO_BUILDDIR)
	$(PROTOC) $(OTEL_PROTO_PROTOC_FLAGS) --grpc-cpp_out=$(OTEL_PROTO_BUILDDIR) $*.proto

modules/grpc/otel/proto/%.pb.cc modules/grpc/otel/proto/%.pb.h:
	mkdir -p $(OTEL_PROTO_BUILDDIR)
	$(PROTOC) $(OTEL_PROTO_PROTOC_FLAGS) $*.proto

OTEL_PROTO_BUILT_SOURCES = \
  $(OTEL_PROTO_BUILDDIR)/opentelemetry/proto/common/v1/common.pb.h \
  $(OTEL_PROTO_BUILDDIR)/opentelemetry/proto/common/v1/common.pb.cc \
  $(OTEL_PROTO_BUILDDIR)/opentelemetry/proto/resource/v1/resource.pb.h \
  $(OTEL_PROTO_BUILDDIR)/opentelemetry/proto/resource/v1/resource.pb.cc \
  $(OTEL_PROTO_BUILDDIR)/opentelemetry/proto/logs/v1/logs.pb.h \
  $(OTEL_PROTO_BUILDDIR)/opentelemetry/proto/logs/v1/logs.pb.cc \
  $(OTEL_PROTO_BUILDDIR)/opentelemetry/proto/metrics/v1/metrics.pb.h \
  $(OTEL_PROTO_BUILDDIR)/opentelemetry/proto/metrics/v1/metrics.pb.cc \
  $(OTEL_PROTO_BUILDDIR)/opentelemetry/proto/trace/v1/trace.pb.h \
  $(OTEL_PROTO_BUILDDIR)/opentelemetry/proto/trace/v1/trace.pb.cc \
  $(OTEL_PROTO_BUILDDIR)/opentelemetry/proto/collector/logs/v1/logs_service.grpc.pb.h \
  $(OTEL_PROTO_BUILDDIR)/opentelemetry/proto/collector/logs/v1/logs_service.grpc.pb.cc \
  $(OTEL_PROTO_BUILDDIR)/opentelemetry/proto/collector/logs/v1/logs_service.pb.h \
  $(OTEL_PROTO_BUILDDIR)/opentelemetry/proto/collector/logs/v1/logs_service.pb.cc \
  $(OTEL_PROTO_BUILDDIR)/opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h \
  $(OTEL_PROTO_BUILDDIR)/opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.cc \
  $(OTEL_PROTO_BUILDDIR)/opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h \
  $(OTEL_PROTO_BUILDDIR)/opentelemetry/proto/collector/metrics/v1/metrics_service.pb.cc \
  $(OTEL_PROTO_BUILDDIR)/opentelemetry/proto/collector/trace/v1/trace_service.grpc.pb.h \
  $(OTEL_PROTO_BUILDDIR)/opentelemetry/proto/collector/trace/v1/trace_service.grpc.pb.cc \
  $(OTEL_PROTO_BUILDDIR)/opentelemetry/proto/collector/trace/v1/trace_service.pb.h \
  $(OTEL_PROTO_BUILDDIR)/opentelemetry/proto/collector/trace/v1/trace_service.pb.cc

noinst_LTLIBRARIES += modules/grpc/otel/libotel_cpp.la

nodist_modules_grpc_otel_libotel_cpp_la_SOURCES = $(OTEL_PROTO_BUILT_SOURCES)

modules_grpc_otel_libotel_cpp_la_SOURCES = \
  modules/grpc/otel/otel-source.h \
  modules/grpc/otel/otel-source.hpp \
  modules/grpc/otel/otel-source.cpp \
  modules/grpc/otel/otel-servicecall.hpp \
  modules/grpc/otel/otel-source-services.hpp \
  modules/grpc/otel/otel-protobuf-parser.h \
  modules/grpc/otel/otel-protobuf-parser.hpp \
  modules/grpc/otel/otel-protobuf-parser.cpp \
  modules/grpc/otel/grpc-credentials-builder.h \
  modules/grpc/otel/grpc-credentials-builder.hpp \
  modules/grpc/otel/grpc-credentials-builder.cpp \
  modules/grpc/otel/otel-protobuf-formatter.hpp \
  modules/grpc/otel/otel-protobuf-formatter.cpp \
  modules/grpc/otel/otel-dest.h \
  modules/grpc/otel/otel-dest.hpp \
  modules/grpc/otel/otel-dest.cpp

modules_grpc_otel_libotel_cpp_la_CXXFLAGS = \
  $(AM_CXXFLAGS) \
  $(PROTOBUF_CFLAGS) \
  $(GRPCPP_CFLAGS) \
  -I$(OTEL_PROTO_BUILDDIR) \
  -I$(top_srcdir)/modules/grpc/otel \
  -I$(top_builddir)/modules/grpc/otel

if ENABLE_EXTRA_WARNINGS
modules_grpc_otel_libotel_cpp_la_CXXFLAGS += -Wno-switch-default
endif

modules_grpc_otel_libotel_cpp_la_LIBADD = $(MODULE_DEPS_LIBS) $(PROTOBUF_LIBS) $(GRPCPP_LIBS)
modules_grpc_otel_libotel_cpp_la_LDFLAGS = $(MODULE_LDFLAGS)
modules_grpc_otel_libotel_cpp_la_DEPENDENCIES = $(MODULE_DEPS_LIBS)

module_LTLIBRARIES += modules/grpc/otel/libotel.la

modules_grpc_otel_libotel_la_SOURCES = \
  modules/grpc/otel/otel-plugin.c \
  modules/grpc/otel/otel-grammar.y \
  modules/grpc/otel/otel-parser.c \
  modules/grpc/otel/otel-parser.h

modules_grpc_otel_libotel_la_CPPFLAGS = \
  $(AM_CPPFLAGS) \
  -I$(top_srcdir)/modules/grpc/otel \
  -I$(top_builddir)/modules/grpc/otel

modules_grpc_otel_libotel_la_LIBADD = \
  $(MODULE_DEPS_LIBS) \
  $(top_builddir)/modules/grpc/otel/libotel_cpp.la \
  -lstdc++

modules_grpc_otel_libotel_la_LDFLAGS = $(MODULE_LDFLAGS)

modules_grpc_otel_libotel_la_DEPENDENCIES = \
  $(MODULE_DEPS_LIBS) \
  $(top_builddir)/modules/grpc/otel/libotel_cpp.la

modules/grpc/otel modules/grpc/otel/ mod-otel: modules/grpc/otel/libotel.la
else
modules/grpc/otel modules/grpc/otel/ mod-otel:
endif

CLEANFILES += \
  $(OTEL_PROTO_BUILT_SOURCES)

BUILT_SOURCES += \
  modules/grpc/otel/otel-grammar.y \
  modules/grpc/otel/otel-grammar.c \
  modules/grpc/otel/otel-grammar.h \
  $(OTEL_PROTO_BUILT_SOURCES)

NODIST_BUILT_SOURCES += \
  $(OTEL_PROTO_BUILT_SOURCES)

EXTRA_DIST += \
  modules/grpc/otel/otel-grammar.ym \
  modules/grpc/otel/CMakeLists.txt \
  $(OTEL_PROTO_SRCDIR)

.PHONY: modules/grpc/otel/ mod-otel

include modules/grpc/otel/tests/Makefile.am
