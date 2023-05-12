if ENABLE_GRPC

noinst_LTLIBRARIES += modules/grpc/otel/libotel_cpp.la

modules_grpc_otel_libotel_cpp_la_SOURCES = \
  modules/grpc/otel/otel-source.h \
  modules/grpc/otel/otel-source.hpp \
  modules/grpc/otel/otel-source.cpp

modules_grpc_otel_libotel_cpp_la_CXXFLAGS = \
  $(AM_CXXFLAGS) \
  -I$(top_srcdir)/modules/grpc/otel \
  -I$(top_builddir)/modules/grpc/otel

modules_grpc_otel_libotel_cpp_la_LIBADD = $(MODULE_DEPS_LIBS)
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

BUILT_SOURCES += \
  modules/grpc/otel/otel-grammar.y \
  modules/grpc/otel/otel-grammar.c \
  modules/grpc/otel/otel-grammar.h

EXTRA_DIST += \
  modules/grpc/otel/otel-grammar.ym \
  modules/grpc/otel/CMakeLists.txt

.PHONY: modules/grpc/otel/ mod-otel
