if ENABLE_GRPC

if ! OS_TYPE_MACOS
modules_grpc_otel_tests_TESTS = \
  modules/grpc/otel/tests/test_otel_protobuf_parser \
  modules/grpc/otel/tests/test_otel_protobuf_formatter \
  modules/grpc/otel/tests/test_syslog_ng_otlp \
  modules/grpc/otel/tests/test_otel_filterx

check_PROGRAMS += ${modules_grpc_otel_tests_TESTS}
endif

modules_grpc_otel_tests_test_otel_protobuf_parser_SOURCES = \
  modules/grpc/otel/tests/test-otel-protobuf-parser.cpp

EXTRA_modules_grpc_otel_tests_test_otel_protobuf_parser_DEPENDENCIES = \
  $(top_builddir)/modules/grpc/otel/libotel_cpp.la \
  $(top_builddir)/modules/grpc/protos/libgrpc-protos.la

modules_grpc_otel_tests_test_otel_protobuf_parser_CXXFLAGS = \
  $(TEST_CXXFLAGS) \
  $(PROTOBUF_CFLAGS) $(GRPCPP_CFLAGS) \
  -I$(OPENTELEMETRY_PROTO_BUILDDIR) \
  -I$(top_srcdir)/modules/grpc/otel \
  -I$(top_builddir)/modules/grpc/otel

modules_grpc_otel_tests_test_otel_protobuf_parser_LDADD = \
  $(TEST_LDADD) \
  $(top_builddir)/modules/grpc/otel/libotel_cpp.la \
  $(top_builddir)/modules/grpc/protos/libgrpc-protos.la

modules_grpc_otel_tests_test_otel_protobuf_formatter_SOURCES = \
  modules/grpc/otel/tests/test-otel-protobuf-formatter.cpp

EXTRA_modules_grpc_otel_tests_test_otel_protobuf_formatter_DEPENDENCIES = \
  $(top_builddir)/modules/grpc/otel/libotel_cpp.la \
  $(top_builddir)/modules/grpc/protos/libgrpc-protos.la

modules_grpc_otel_tests_test_otel_protobuf_formatter_CXXFLAGS = \
  $(TEST_CXXFLAGS) \
  $(PROTOBUF_CFLAGS) $(GRPCPP_CFLAGS) \
  -I$(OPENTELEMETRY_PROTO_BUILDDIR) \
  -I$(top_srcdir)/modules/grpc/otel \
  -I$(top_builddir)/modules/grpc/otel

modules_grpc_otel_tests_test_otel_protobuf_formatter_LDADD = \
  $(TEST_LDADD) \
  $(top_builddir)/modules/grpc/otel/libotel_cpp.la \
  $(top_builddir)/modules/grpc/protos/libgrpc-protos.la

modules_grpc_otel_tests_test_syslog_ng_otlp_SOURCES = \
  modules/grpc/otel/tests/test-syslog-ng-otlp.cpp

EXTRA_modules_grpc_otel_tests_test_syslog_ng_otlp_DEPENDENCIES = \
  $(top_builddir)/modules/grpc/otel/libotel_cpp.la \
  $(top_builddir)/modules/grpc/protos/libgrpc-protos.la

modules_grpc_otel_tests_test_syslog_ng_otlp_CXXFLAGS = \
  $(TEST_CXXFLAGS) \
  $(PROTOBUF_CFLAGS) $(GRPCPP_CFLAGS) \
  -I$(OPENTELEMETRY_PROTO_BUILDDIR) \
  -I$(top_srcdir)/modules/grpc/otel \
  -I$(top_builddir)/modules/grpc/otel

modules_grpc_otel_tests_test_syslog_ng_otlp_LDADD = \
  $(TEST_LDADD) \
  $(top_builddir)/modules/grpc/otel/libotel_cpp.la \
  $(top_builddir)/modules/grpc/protos/libgrpc-protos.la

modules_grpc_otel_tests_test_otel_filterx_SOURCES = \
  modules/grpc/otel/tests/test-otel-filterx.cpp

EXTRA_modules_grpc_otel_tests_test_otel_filterx_DEPENDENCIES = \
  $(top_builddir)/modules/grpc/otel/libotel_cpp.la \
  $(top_builddir)/modules/grpc/protos/libgrpc-protos.la

modules_grpc_otel_tests_test_otel_filterx_CXXFLAGS = \
  $(TEST_CXXFLAGS) \
  $(PROTOBUF_CFLAGS) $(GRPCPP_CFLAGS) \
  -I$(OPENTELEMETRY_PROTO_BUILDDIR) \
  -I$(top_srcdir)/modules/grpc/otel \
  -I$(top_builddir)/modules/grpc/otel

modules_grpc_otel_tests_test_otel_filterx_LDADD = \
  $(TEST_LDADD) \
  $(top_builddir)/modules/grpc/otel/libotel_cpp.la \
  $(top_builddir)/modules/grpc/protos/libgrpc-protos.la \
  $(top_builddir)/modules/grpc/otel/filterx/libfilterx.la

endif

EXTRA_DIST += \
    modules/grpc/otel/tests/CMakeLists.txt
