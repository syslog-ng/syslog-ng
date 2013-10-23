#include "mock-transport.h"
#include "proto_lib.h"
#include "msg_parse_lib.h"
#include "logproto/logproto-regexp-multiline-server.h"

/****************************************************************************************
 * LogProtoREMultiLineServer
 ****************************************************************************************/

static regex_t *
compile_regex(const gchar *re)
{
  regex_t *preg = g_new(regex_t, 1);

  if (regcomp(preg, re, REG_EXTENDED) < 0)
    g_assert_not_reached();
  return preg;
}

static void
test_lines_separated_with_prefix(gboolean input_is_stream)
{
  LogProtoServer *proto;

  proto = log_proto_regexp_multiline_server_new(
            /* 32 bytes max line length, which means that the complete
             * multi-line block plus one additional line must fit into 32
             * bytes. */
            (input_is_stream ? log_transport_mock_stream_new : log_transport_mock_records_new)(
              "Foo First Line\n"
              "Foo Second Line\n"
              "Foo Third Line\n"
              "Foo Multiline\n"
              "multi\n"
              "Foo final\n", -1,
              LTM_PADDING,
              LTM_EOF),
            get_inited_proto_server_options(),
            compile_regex("^Foo"), NULL);

  assert_proto_server_fetch(proto, "Foo First Line", -1);
  assert_proto_server_fetch(proto, "Foo Second Line", -1);
  assert_proto_server_fetch(proto, "Foo Third Line", -1);
  assert_proto_server_fetch(proto, "Foo Multiline\nmulti", -1);

  log_proto_server_free(proto);
}

static void
test_lines_separated_with_prefix_and_garbage(gboolean input_is_stream)
{
  LogProtoServer *proto;

  proto = log_proto_regexp_multiline_server_new(
            /* 32 bytes max line length, which means that the complete
             * multi-line block plus one additional line must fit into 32
             * bytes. */
            (input_is_stream ? log_transport_mock_stream_new : log_transport_mock_records_new)(
              "Foo First Line Bar\n"
              "Foo Second Line Bar\n"
              "Foo Third Line Bar\n"
              "Foo Multiline\n"
              "multi Bar\n"
              "Foo final\n", -1,
              LTM_PADDING,
              LTM_EOF),
            get_inited_proto_server_options(),
            compile_regex("^Foo"), compile_regex(" Bar$"));

  assert_proto_server_fetch(proto, "Foo First Line", -1);
  assert_proto_server_fetch(proto, "Foo Second Line", -1);
  assert_proto_server_fetch(proto, "Foo Third Line", -1);
  assert_proto_server_fetch(proto, "Foo Multiline\nmulti", -1);

  log_proto_server_free(proto);
}

static void
test_lines_separated_with_garbage(gboolean input_is_stream)
{
  LogProtoServer *proto;

  proto = log_proto_regexp_multiline_server_new(
            /* 32 bytes max line length, which means that the complete
             * multi-line block plus one additional line must fit into 32
             * bytes. */
            (input_is_stream ? log_transport_mock_stream_new : log_transport_mock_records_new)(
              "Foo First Line Bar\n"
              "Foo Second Line Bar\n"
              "Foo Third Line Bar\n"
              "Foo Multiline\n"
              "multi Bar\n"
              "Foo final\n", -1,
              LTM_PADDING,
              LTM_EOF),
            get_inited_proto_server_options(),
            NULL, compile_regex(" Bar$"));

  assert_proto_server_fetch(proto, "Foo First Line", -1);
  assert_proto_server_fetch(proto, "Foo Second Line", -1);
  assert_proto_server_fetch(proto, "Foo Third Line", -1);
  assert_proto_server_fetch(proto, "Foo Multiline\nmulti", -1);

  log_proto_server_free(proto);
}

static void
test_first_line_without_prefix(gboolean input_is_stream)
{
  LogProtoServer *proto;

  proto = log_proto_regexp_multiline_server_new(
            /* 32 bytes max line length, which means that the complete
             * multi-line block plus one additional line must fit into 32
             * bytes. */
            (input_is_stream ? log_transport_mock_stream_new : log_transport_mock_records_new)(
              "First Line\n"
              "Foo Second Line\n"
              "Foo Third Line\n"
              "Foo Multiline\n"
              "multi\n"
              "Foo final\n", -1,
              LTM_PADDING,
              LTM_EOF),
            get_inited_proto_server_options(),
            compile_regex("^Foo"), NULL);

  assert_proto_server_fetch(proto, "First Line", -1);
  assert_proto_server_fetch(proto, "Foo Second Line", -1);
  assert_proto_server_fetch(proto, "Foo Third Line", -1);
  assert_proto_server_fetch(proto, "Foo Multiline\nmulti", -1);

  log_proto_server_free(proto);
}

void
test_log_proto_regexp_multiline_server(void)
{
  PROTO_TESTCASE(test_lines_separated_with_prefix, FALSE);
  PROTO_TESTCASE(test_lines_separated_with_prefix, TRUE);
  PROTO_TESTCASE(test_lines_separated_with_prefix_and_garbage, FALSE);
  PROTO_TESTCASE(test_lines_separated_with_prefix_and_garbage, TRUE);
  PROTO_TESTCASE(test_lines_separated_with_garbage, FALSE);
  PROTO_TESTCASE(test_lines_separated_with_garbage, TRUE);
  PROTO_TESTCASE(test_first_line_without_prefix, FALSE);
  PROTO_TESTCASE(test_first_line_without_prefix, TRUE);
}
