if ENABLE_GRPC

noinst_LTLIBRARIES += modules/grpc/otel/libotel_cpp.la

modules_grpc_otel_libotel_cpp_la_SOURCES = \
  modules/grpc/otel/otel-source.h \
  modules/grpc/otel/otel-source.hpp \
  modules/grpc/otel/otel-source.cpp \
  modules/grpc/otel/otel-servicecall.hpp \
  modules/grpc/otel/otel-source-services.hpp \
  modules/grpc/otel/otel-logmsg-handles.h \
  modules/grpc/otel/otel-logmsg-handles.hpp \
  modules/grpc/otel/otel-logmsg-handles.cpp \
  modules/grpc/otel/otel-protobuf-parser.h \
  modules/grpc/otel/otel-protobuf-parser.hpp \
  modules/grpc/otel/otel-protobuf-parser.cpp \
  modules/grpc/otel/otel-protobuf-formatter.hpp \
  modules/grpc/otel/otel-protobuf-formatter.cpp \
  modules/grpc/otel/otel-dest.h \
  modules/grpc/otel/otel-dest.hpp \
  modules/grpc/otel/otel-dest.cpp \
  modules/grpc/otel/otel-dest-worker.hpp \
  modules/grpc/otel/otel-dest-worker.cpp \
  modules/grpc/otel/syslog-ng-otlp-dest.h \
  modules/grpc/otel/syslog-ng-otlp-dest.hpp \
  modules/grpc/otel/syslog-ng-otlp-dest.cpp \
  modules/grpc/otel/syslog-ng-otlp-dest-worker.hpp \
  modules/grpc/otel/syslog-ng-otlp-dest-worker.cpp

modules_grpc_otel_libotel_cpp_la_CXXFLAGS = \
  $(AM_CXXFLAGS) \
  -Wno-double-promotion -Wno-deprecated -DPROTOBUF_ENABLE_DEBUG_LOGGING_MAY_LEAK_PII=0 \
  $(PROTOBUF_CFLAGS) \
  $(GRPCPP_CFLAGS) \
  $(GRPC_COMMON_CFLAGS) \
  -I$(OPENTELEMETRY_PROTO_BUILDDIR) \
  -I$(top_srcdir)/modules/grpc \
  -I$(top_srcdir)/modules/grpc/otel \
  -I$(top_builddir)/modules/grpc/otel

modules_grpc_otel_libotel_cpp_la_LIBADD = $(MODULE_DEPS_LIBS) $(PROTOBUF_LIBS) $(GRPCPP_LIBS)
modules_grpc_otel_libotel_cpp_la_LDFLAGS =  $(MODULE_LDFLAGS)
EXTRA_modules_grpc_otel_libotel_cpp_la_DEPENDENCIES = $(MODULE_DEPS_LIBS)

module_LTLIBRARIES += modules/grpc/otel/libotel.la

modules_grpc_otel_libotel_la_SOURCES = \
  modules/grpc/otel/otel-plugin.c \
  modules/grpc/otel/otel-grammar.y \
  modules/grpc/otel/otel-parser.c \
  modules/grpc/otel/otel-parser.h

modules_grpc_otel_libotel_la_CPPFLAGS = \
  $(AM_CPPFLAGS) \
  $(GRPC_COMMON_CFLAGS) \
  -I$(top_srcdir)/modules/grpc \
  -I$(top_srcdir)/modules/grpc/otel \
  -I$(top_builddir)/modules/grpc/otel

modules_grpc_otel_libotel_la_LIBADD = \
  $(MODULE_DEPS_LIBS) \
  $(GRPC_COMMON_LIBS) \
  $(top_builddir)/modules/grpc/protos/libgrpc-protos.la \
  $(top_builddir)/modules/grpc/otel/libotel_cpp.la

nodist_EXTRA_modules_grpc_otel_libotel_la_SOURCES = force-cpp-linker-with-default-stdlib.cpp

modules_grpc_otel_libotel_la_LDFLAGS = $(MODULE_LDFLAGS)

EXTRA_modules_grpc_otel_libotel_la_DEPENDENCIES = \
  $(MODULE_DEPS_LIBS) \
  $(GRPC_COMMON_LIBS) \
  $(top_builddir)/modules/grpc/protos/libgrpc-protos.la \
  $(top_builddir)/modules/grpc/otel/libotel_cpp.la

modules/grpc/otel modules/grpc/otel/ mod-otel: modules/grpc/otel/libotel.la
else
modules/grpc/otel modules/grpc/otel/ mod-otel:
endif

modules/grpc/otel/otel-grammar.y: modules/grpc/common/grpc-grammar.ym

BUILT_SOURCES += \
  modules/grpc/otel/otel-grammar.y \
  modules/grpc/otel/otel-grammar.c \
  modules/grpc/otel/otel-grammar.h

EXTRA_DIST += \
  modules/grpc/otel/otel-grammar.ym \
  modules/grpc/otel/CMakeLists.txt

.PHONY: modules/grpc/otel/ mod-otel

include modules/grpc/otel/tests/Makefile.am
