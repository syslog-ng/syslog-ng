if ENABLE_GRPC

# noinst: Built as part of a larger libotel module as static library
noinst_LTLIBRARIES += modules/grpc/otel/filterx/libfilterx.la

modules_grpc_otel_filterx_libfilterx_la_SOURCES = \
  modules/grpc/otel/filterx/object-otel.h \
  modules/grpc/otel/filterx/object-otel-logrecord.cpp \
  modules/grpc/otel/filterx/object-otel-logrecord.hpp \
  modules/grpc/otel/filterx/object-otel-resource.cpp \
  modules/grpc/otel/filterx/object-otel-resource.hpp \
  modules/grpc/otel/filterx/object-otel-scope.cpp \
  modules/grpc/otel/filterx/object-otel-scope.hpp \
  modules/grpc/otel/filterx/object-otel-kvlist.cpp \
  modules/grpc/otel/filterx/object-otel-kvlist.hpp \
  modules/grpc/otel/filterx/object-otel-array.cpp \
  modules/grpc/otel/filterx/object-otel-array.hpp \
  modules/grpc/otel/filterx/protobuf-field.cpp \
  modules/grpc/otel/filterx/protobuf-field.hpp \
  modules/grpc/otel/filterx/otel-field.cpp \
  modules/grpc/otel/filterx/otel-field.hpp

modules_grpc_otel_filterx_libfilterx_la_CXXFLAGS = \
  $(AM_CXXFLAGS) \
  $(PROTOBUF_CFLAGS) \
  $(GRPCPP_CFLAGS) \
  -I$(OPENTELEMETRY_PROTO_BUILDDIR) \
  -I$(top_srcdir)/modules/grpc/otel/filterx \
  -I$(top_builddir)/modules/grpc/otel/filterx \
  -Wno-deprecated-declarations

modules_grpc_otel_filterx_libfilterx_la_LIBADD = \
  $(MODULE_DEPS_LIBS) \
  $(PROTOBUF_LIBS) \
  $(top_builddir)/modules/grpc/protos/libgrpc-protos.la

modules_grpc_otel_filterx_libfilterx_la_LDFLAGS = $(MODULE_LDFLAGS)

EXTRA_modules_grpc_otel_filterx_libfilterx_la_DEPENDENCIES = \
  $(MODULE_DEPS_LIBS) \
  $(top_builddir)/modules/grpc/protos/libgrpc-protos.la

endif

EXTRA_DIST += \
  modules/grpc/otel/filterx/CMakeLists.txt
