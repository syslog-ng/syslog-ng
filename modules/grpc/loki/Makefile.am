if ENABLE_GRPC

noinst_LTLIBRARIES += modules/grpc/loki/libloki_cpp.la

modules_grpc_loki_libloki_cpp_la_SOURCES = \
  $(grpc_credentials_sources) \
  $(grpc_metrics_sources) \
  modules/grpc/loki/loki-dest.h \
  modules/grpc/loki/loki-dest.hpp \
  modules/grpc/loki/loki-dest.cpp \
  modules/grpc/loki/loki-worker.h \
  modules/grpc/loki/loki-worker.hpp \
  modules/grpc/loki/loki-worker.cpp

modules_grpc_loki_libloki_cpp_la_CXXFLAGS = \
  $(AM_CXXFLAGS) \
  $(PROTOBUF_CFLAGS) \
  $(GRPCPP_CFLAGS) \
  -I$(LOKI_PROTO_BUILDDIR) \
  -I$(top_srcdir)/modules/grpc \
  -I$(top_srcdir)/modules/grpc/loki \
  -I$(top_builddir)/modules/grpc/loki

modules_grpc_loki_libloki_cpp_la_LIBADD = $(MODULE_DEPS_LIBS) $(PROTOBUF_LIBS) $(GRPCPP_LIBS)
modules_grpc_loki_libloki_cpp_la_LDFLAGS = $(MODULE_LDFLAGS)
EXTRA_modules_grpc_loki_libloki_cpp_la_DEPENDENCIES = $(MODULE_DEPS_LIBS)

module_LTLIBRARIES += modules/grpc/loki/libloki.la

modules_grpc_loki_libloki_la_SOURCES = \
  modules/grpc/loki/loki-grammar.y \
  modules/grpc/loki/loki-parser.c \
  modules/grpc/loki/loki-parser.h \
  modules/grpc/loki/loki-plugin.c

modules_grpc_loki_libloki_la_CPPFLAGS = \
  $(AM_CPPFLAGS) \
  -I$(top_srcdir)/modules/grpc \
  -I$(top_srcdir)/modules/grpc/loki \
  -I$(top_builddir)/modules/grpc/loki \
  -I$(top_srcdir)/modules/grpc

modules_grpc_loki_libloki_la_LIBADD = \
  $(MODULE_DEPS_LIBS) \
  $(top_builddir)/modules/grpc/protos/libgrpc-protos.la \
  $(top_builddir)/modules/grpc/loki/libloki_cpp.la

nodist_EXTRA_modules_grpc_loki_libloki_la_SOURCES = force-cpp-linker-with-default-stdlib.cpp

modules_grpc_loki_libloki_la_LDFLAGS = $(MODULE_LDFLAGS)
EXTRA_modules_grpc_loki_libloki_la_DEPENDENCIES = \
  $(MODULE_DEPS_LIBS) \
  $(top_builddir)/modules/grpc/protos/libgrpc-protos.la \
  $(top_builddir)/modules/grpc/loki/libloki_cpp.la

modules/grpc/loki modules/grpc/loki/ mod-loki: modules/grpc/loki/libloki.la

else
modules/grpc/loki modules/grpc/loki/ mod-loki:
endif

BUILT_SOURCES += \
  modules/grpc/loki/loki-grammar.y \
  modules/grpc/loki/loki-grammar.c \
  modules/grpc/loki/loki-grammar.h

EXTRA_DIST += \
  modules/grpc/loki/loki-grammar.ym \
  modules/grpc/loki/CMakeLists.txt

.PHONY: modules/grpc/loki/ mod-loki
