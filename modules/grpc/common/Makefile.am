include modules/grpc/common/credentials/Makefile.am
include modules/grpc/common/metrics/Makefile.am
include modules/grpc/common/schema/Makefile.am

if ENABLE_GRPC

noinst_LTLIBRARIES += modules/grpc/common/libgrpc-common.la

GRPC_COMMON_CFLAGS = \
  -I$(top_srcdir)/modules/grpc/common \
  -I$(top_builddir)/modules/grpc/common

GRPC_COMMON_LIBS = \
  $(top_builddir)/modules/grpc/common/libgrpc-common.la


modules_grpc_common_libgrpc_common_la_SOURCES = \
  $(grpc_credentials_sources) \
  $(grpc_metrics_sources) \
  $(grpc_schema_sources) \
  modules/grpc/common/grpc-parser.h \
  modules/grpc/common/grpc-dest.h \
  modules/grpc/common/grpc-dest.hpp \
  modules/grpc/common/grpc-dest.cpp \
  modules/grpc/common/grpc-dest-worker.hpp \
  modules/grpc/common/grpc-dest-worker.cpp \
  modules/grpc/common/grpc-source.h \
  modules/grpc/common/grpc-source.hpp \
  modules/grpc/common/grpc-source.cpp \
  modules/grpc/common/grpc-source-worker.hpp \
  modules/grpc/common/grpc-source-worker.cpp

modules_grpc_common_libgrpc_common_la_CXXFLAGS = \
  $(AM_CXXFLAGS) \
  $(PROTOBUF_CFLAGS) \
  $(GRPCPP_CFLAGS) \
  $(GRPC_COMMON_CFLAGS)

modules_grpc_common_libgrpc_common_la_LIBADD = $(MODULE_DEPS_LIBS) $(PROTOBUF_LIBS) $(GRPCPP_LIBS)
modules_grpc_common_libgrpc_common_la_LDFLAGS = $(MODULE_LDFLAGS)
EXTRA_modules_grpc_common_libgrpc_common_la_DEPENDENCIES = $(MODULE_DEPS_LIBS)

endif

EXTRA_DIST += \
  modules/grpc/common/CMakeLists.txt \
  modules/grpc/common/grpc-grammar.ym
