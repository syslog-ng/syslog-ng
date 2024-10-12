if ENABLE_GRPC

noinst_LTLIBRARIES += modules/grpc/clickhouse/libclickhouse_cpp.la

modules_grpc_clickhouse_libclickhouse_cpp_la_SOURCES = \
  modules/grpc/clickhouse/clickhouse-dest.h \
  modules/grpc/clickhouse/clickhouse-dest.hpp \
  modules/grpc/clickhouse/clickhouse-dest.cpp \
  modules/grpc/clickhouse/clickhouse-dest-worker.hpp \
  modules/grpc/clickhouse/clickhouse-dest-worker.cpp

modules_grpc_clickhouse_libclickhouse_cpp_la_CXXFLAGS = \
  $(AM_CXXFLAGS) \
  $(PROTOBUF_CFLAGS) \
  $(GRPCPP_CFLAGS) \
  $(GRPC_COMMON_CFLAGS) \
  -I$(CLICKHOUSE_PROTO_BUILDDIR) \
  -I$(top_srcdir)/modules/grpc \
  -I$(top_srcdir)/modules/grpc/clickhouse \
  -I$(top_builddir)/modules/grpc/clickhouse

modules_grpc_clickhouse_libclickhouse_cpp_la_LIBADD = $(MODULE_DEPS_LIBS) $(PROTOBUF_LIBS) $(GRPCPP_LIBS)
modules_grpc_clickhouse_libclickhouse_cpp_la_LDFLAGS = $(MODULE_LDFLAGS)
EXTRA_modules_grpc_clickhouse_libclickhouse_cpp_la_DEPENDENCIES = $(MODULE_DEPS_LIBS)

module_LTLIBRARIES += modules/grpc/clickhouse/libclickhouse.la

modules_grpc_clickhouse_libclickhouse_la_SOURCES = \
  modules/grpc/clickhouse/clickhouse-grammar.y \
  modules/grpc/clickhouse/clickhouse-parser.c \
  modules/grpc/clickhouse/clickhouse-parser.h \
  modules/grpc/clickhouse/clickhouse-plugin.c

modules_grpc_clickhouse_libclickhouse_la_CPPFLAGS = \
  $(AM_CPPFLAGS) \
  $(GRPC_COMMON_CFLAGS) \
  -I$(top_srcdir)/modules/grpc/clickhouse \
  -I$(top_builddir)/modules/grpc/clickhouse \
  -I$(top_srcdir)/modules/grpc

modules_grpc_clickhouse_libclickhouse_la_LIBADD = \
  $(MODULE_DEPS_LIBS) \
  $(GRPC_COMMON_LIBS) \
  $(top_builddir)/modules/grpc/protos/libgrpc-protos.la \
  $(top_builddir)/modules/grpc/clickhouse/libclickhouse_cpp.la

nodist_EXTRA_modules_grpc_clickhouse_libclickhouse_la_SOURCES = force-cpp-linker-with-default-stdlib.cpp

modules_grpc_clickhouse_libclickhouse_la_LDFLAGS = $(MODULE_LDFLAGS)
EXTRA_modules_grpc_clickhouse_libclickhouse_la_DEPENDENCIES = \
  $(MODULE_DEPS_LIBS) \
  $(GRPC_COMMON_LIBS) \
  $(top_builddir)/modules/grpc/protos/libgrpc-protos.la \
  $(top_builddir)/modules/grpc/clickhouse/libclickhouse_cpp.la

modules/grpc/clickhouse modules/grpc/clickhouse/ mod-clickhouse: modules/grpc/clickhouse/libclickhouse.la

else
modules/grpc/clickhouse modules/grpc/clickhouse/ mod-clickhouse:
endif

BUILT_SOURCES += \
  modules/grpc/clickhouse/clickhouse-grammar.y \
  modules/grpc/clickhouse/clickhouse-grammar.c \
  modules/grpc/clickhouse/clickhouse-grammar.h

EXTRA_DIST += \
  modules/grpc/clickhouse/clickhouse-grammar.ym \
  modules/grpc/clickhouse/CMakeLists.txt

.PHONY: modules/grpc/clickhouse/ mod-clickhouse
