add_unit_test(
  CRITERION
  TARGET test_otel_protobuf_parser
  SOURCES test-otel-protobuf-parser.cpp
  INCLUDES ${OTEL_PROTO_BUILDDIR}
  DEPENDS otel-cpp)

add_unit_test(
  CRITERION
  TARGET test_otel_protobuf_formatter
  SOURCES test-otel-protobuf-formatter.cpp
  INCLUDES ${OTEL_PROTO_BUILDDIR}
  DEPENDS otel-cpp)
