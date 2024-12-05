if ENABLE_GRPC

noinst_LTLIBRARIES += modules/grpc/pubsub/libpubsub_cpp.la

modules_grpc_pubsub_libpubsub_cpp_la_SOURCES = \
  modules/grpc/pubsub/pubsub-dest.h \
  modules/grpc/pubsub/pubsub-dest.hpp \
  modules/grpc/pubsub/pubsub-dest.cpp \
  modules/grpc/pubsub/pubsub-dest-worker.hpp \
  modules/grpc/pubsub/pubsub-dest-worker.cpp

modules_grpc_pubsub_libpubsub_cpp_la_CXXFLAGS = \
  $(AM_CXXFLAGS) \
  $(PROTOBUF_CFLAGS) \
  $(GRPCPP_CFLAGS) \
  $(GRPC_COMMON_CFLAGS) \
  -I$(GOOGLEAPIS_PROTO_BUILDDIR) \
  -I$(top_srcdir)/modules/grpc \
  -I$(top_srcdir)/modules/grpc/pubsub \
  -I$(top_builddir)/modules/grpc/pubsub

modules_grpc_pubsub_libpubsub_cpp_la_LIBADD = $(MODULE_DEPS_LIBS) $(PROTOBUF_LIBS) $(GRPCPP_LIBS)
modules_grpc_pubsub_libpubsub_cpp_la_LDFLAGS = $(MODULE_LDFLAGS)
EXTRA_modules_grpc_pubsub_libpubsub_cpp_la_DEPENDENCIES = $(MODULE_DEPS_LIBS)

module_LTLIBRARIES += modules/grpc/pubsub/libpubsub.la

modules_grpc_pubsub_libpubsub_la_SOURCES = \
  modules/grpc/pubsub/pubsub-grammar.y \
  modules/grpc/pubsub/pubsub-parser.c \
  modules/grpc/pubsub/pubsub-parser.h \
  modules/grpc/pubsub/pubsub-plugin.c

modules_grpc_pubsub_libpubsub_la_CPPFLAGS = \
  $(AM_CPPFLAGS) \
  $(GRPC_COMMON_CFLAGS) \
  -I$(top_srcdir)/modules/grpc/pubsub \
  -I$(top_builddir)/modules/grpc/pubsub \
  -I$(top_srcdir)/modules/grpc

modules_grpc_pubsub_libpubsub_la_LIBADD = \
  $(MODULE_DEPS_LIBS) \
  $(GRPC_COMMON_LIBS) \
  $(top_builddir)/modules/grpc/protos/libgrpc-protos.la \
  $(top_builddir)/modules/grpc/pubsub/libpubsub_cpp.la

nodist_EXTRA_modules_grpc_pubsub_libpubsub_la_SOURCES = force-cpp-linker-with-default-stdlib.cpp

modules_grpc_pubsub_libpubsub_la_LDFLAGS = $(MODULE_LDFLAGS)
EXTRA_modules_grpc_pubsub_libpubsub_la_DEPENDENCIES = \
  $(MODULE_DEPS_LIBS) \
  $(GRPC_COMMON_LIBS) \
  $(top_builddir)/modules/grpc/protos/libgrpc-protos.la \
  $(top_builddir)/modules/grpc/pubsub/libpubsub_cpp.la

modules/grpc/pubsub modules/grpc/pubsub/ mod-pubsub: modules/grpc/pubsub/libpubsub.la

else
modules/grpc/pubsub modules/grpc/pubsub/ mod-pubsub:
endif

modules/grpc/pubsub/pubsub-grammar.y: modules/grpc/common/grpc-grammar.ym

BUILT_SOURCES += \
  modules/grpc/pubsub/pubsub-grammar.y \
  modules/grpc/pubsub/pubsub-grammar.c \
  modules/grpc/pubsub/pubsub-grammar.h

EXTRA_DIST += \
  modules/grpc/pubsub/pubsub-grammar.ym \
  modules/grpc/pubsub/CMakeLists.txt

.PHONY: modules/grpc/pubsub/ mod-pubsub
