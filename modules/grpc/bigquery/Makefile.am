if ENABLE_GRPC

noinst_LTLIBRARIES += modules/grpc/bigquery/libbigquery_cpp.la

modules_grpc_bigquery_libbigquery_cpp_la_SOURCES = \
  $(grpc_metrics_sources) \
  modules/grpc/bigquery/bigquery-dest.h \
  modules/grpc/bigquery/bigquery-dest.hpp \
  modules/grpc/bigquery/bigquery-dest.cpp \
  modules/grpc/bigquery/bigquery-worker.h \
  modules/grpc/bigquery/bigquery-worker.hpp \
  modules/grpc/bigquery/bigquery-worker.cpp

modules_grpc_bigquery_libbigquery_cpp_la_CXXFLAGS = \
  $(AM_CXXFLAGS) \
  $(PROTOBUF_CFLAGS) \
  $(GRPCPP_CFLAGS) \
  -I$(GOOGLEAPIS_PROTO_BUILDDIR) \
  -I$(top_srcdir)/modules/grpc \
  -I$(top_srcdir)/modules/grpc/bigquery \
  -I$(top_builddir)/modules/grpc/bigquery

modules_grpc_bigquery_libbigquery_cpp_la_LIBADD = $(MODULE_DEPS_LIBS) $(PROTOBUF_LIBS) $(GRPCPP_LIBS)
modules_grpc_bigquery_libbigquery_cpp_la_LDFLAGS = $(MODULE_LDFLAGS)
EXTRA_modules_grpc_bigquery_libbigquery_cpp_la_DEPENDENCIES = $(MODULE_DEPS_LIBS)

module_LTLIBRARIES += modules/grpc/bigquery/libbigquery.la

modules_grpc_bigquery_libbigquery_la_SOURCES = \
  modules/grpc/bigquery/bigquery-grammar.y \
  modules/grpc/bigquery/bigquery-parser.c \
  modules/grpc/bigquery/bigquery-parser.h \
  modules/grpc/bigquery/bigquery-plugin.c

modules_grpc_bigquery_libbigquery_la_CPPFLAGS = \
  $(AM_CPPFLAGS) \
  -I$(top_srcdir)/modules/grpc/bigquery \
  -I$(top_builddir)/modules/grpc/bigquery \
  -I$(top_srcdir)/modules/grpc

modules_grpc_bigquery_libbigquery_la_LIBADD = \
  $(MODULE_DEPS_LIBS) \
  $(top_builddir)/modules/grpc/protos/libgrpc-protos.la \
  $(top_builddir)/modules/grpc/bigquery/libbigquery_cpp.la

nodist_EXTRA_modules_grpc_bigquery_libbigquery_la_SOURCES = force-cpp-linker-with-default-stdlib.cpp

modules_grpc_bigquery_libbigquery_la_LDFLAGS = $(MODULE_LDFLAGS)
EXTRA_modules_grpc_bigquery_libbigquery_la_DEPENDENCIES = \
  $(MODULE_DEPS_LIBS) \
  $(top_builddir)/modules/grpc/protos/libgrpc-protos.la \
  $(top_builddir)/modules/grpc/bigquery/libbigquery_cpp.la

modules/grpc/bigquery modules/grpc/bigquery/ mod-bigquery: modules/grpc/bigquery/libbigquery.la

else
modules/grpc/bigquery modules/grpc/bigquery/ mod-bigquery:
endif

BUILT_SOURCES += \
  modules/grpc/bigquery/bigquery-grammar.y \
  modules/grpc/bigquery/bigquery-grammar.c \
  modules/grpc/bigquery/bigquery-grammar.h

EXTRA_DIST += \
  modules/grpc/bigquery/bigquery-grammar.ym \
  modules/grpc/bigquery/CMakeLists.txt

.PHONY: modules/grpc/bigquery/ mod-bigquery
