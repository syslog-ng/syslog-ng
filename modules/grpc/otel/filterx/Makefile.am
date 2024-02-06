if ENABLE_GRPC

# noinst: Built as part of a larger libotel module as static library
noinst_LTLIBRARIES += modules/grpc/otel/filterx/libfilterx.la

modules_grpc_otel_filterx_libfilterx_la_SOURCES = \
  modules/grpc/otel/filterx/otel-logrecord.cpp \
  modules/grpc/otel/filterx/otel-logrecord.hpp \
  modules/grpc/otel/filterx/otel-logrecord.h

modules_grpc_otel_filterx_libfilterx_la_CXXFLAGS = \
  $(AM_CXXFLAGS) \
  $(PROTOBUF_CFLAGS) \
  $(GRPCPP_CFLAGS) \
  -I$(OPENTELEMETRY_PROTO_BUILDDIR) \
  -I$(top_srcdir)/modules/grpc/otel/filterx \
  -I$(top_builddir)/modules/grpc/otel/filterx

modules_grpc_otel_filterx_libfilterx_la_LIBADD = \
  $(MODULE_DEPS_LIBS) \
  $(PROTOBUF_LIBS) \
  $(top_builddir)/modules/grpc/protos/libgrpc-protos.la

modules_grpc_otel_filterx_libfilterx_la_LDFLAGS = $(MODULE_LDFLAGS)

modules_grpc_otel_filterx_libfilterx_la_DEPENDENCIES = \
  $(MODULE_DEPS_LIBS) \
  $(top_builddir)/modules/grpc/protos/libgrpc-protos.la

endif

EXTRA_DIST += \
  modules/grpc/otel/filterx/CMakeLists.txt
