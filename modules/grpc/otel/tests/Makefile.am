if ENABLE_GRPC

modules_grpc_otel_tests_TESTS = \
  modules/grpc/otel/tests/test_otel_protobuf_parser \
  modules/grpc/otel/tests/test_otel_protobuf_formatter

check_PROGRAMS += ${modules_grpc_otel_tests_TESTS}

modules_grpc_otel_tests_test_otel_protobuf_parser_SOURCES = \
  modules/grpc/otel/tests/test-otel-protobuf-parser.cpp

modules_grpc_otel_tests_test_otel_protobuf_parser_DEPENDENCIES = \
  $(top_builddir)/modules/grpc/otel/libotel_cpp.la

modules_grpc_otel_tests_test_otel_protobuf_parser_CXXFLAGS = \
  $(TEST_CXXFLAGS) \
  -I$(OTEL_PROTO_BUILDDIR) \
  -I$(top_srcdir)/modules/grpc/otel \
  -I$(top_builddir)/modules/grpc/otel

modules_grpc_otel_tests_test_otel_protobuf_parser_LDADD = \
  $(TEST_LDADD) \
  $(top_builddir)/modules/grpc/otel/libotel_cpp.la

modules_grpc_otel_tests_test_otel_protobuf_formatter_SOURCES = \
  modules/grpc/otel/tests/test-otel-protobuf-formatter.cpp

modules_grpc_otel_tests_test_otel_protobuf_formatter_DEPENDENCIES = \
  $(top_builddir)/modules/grpc/otel/libotel_cpp.la

modules_grpc_otel_tests_test_otel_protobuf_formatter_CXXFLAGS = \
  $(TEST_CXXFLAGS) \
  -I$(OTEL_PROTO_BUILDDIR) \
  -I$(top_srcdir)/modules/grpc/otel \
  -I$(top_builddir)/modules/grpc/otel

modules_grpc_otel_tests_test_otel_protobuf_formatter_LDADD = \
  $(TEST_LDADD) \
  $(top_builddir)/modules/grpc/otel/libotel_cpp.la

endif

EXTRA_DIST += \
    modules/grpc/otel/tests/CMakeLists.txt
