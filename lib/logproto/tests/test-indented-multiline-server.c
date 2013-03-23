#include "mock-transport.h"
#include "proto_lib.h"
#include "msg_parse_lib.h"
#include "logproto/logproto-indented-multiline-server.h"

/****************************************************************************************
 * LogProtoIMultiLineServer
 ****************************************************************************************/

static void
test_proper_multiline(gboolean input_is_stream)
{
  LogProtoServer *proto;

  proto_server_options.max_msg_size = 32;

  proto = log_proto_indented_multiline_server_new(
            /* 32 bytes max line length */
            (input_is_stream ? log_transport_mock_stream_new : log_transport_mock_records_new)(
              "0\n"
              " 1=2\n"
              " 3=4\n"
              "newline\n", -1,
              LTM_PADDING,
              LTM_EOF),
            get_inited_proto_server_options());

  assert_proto_server_fetch(proto, "0\n"
                                   " 1=2\n"
                                   " 3=4", -1);

  log_proto_server_free(proto);
}

static void
test_line_without_continuation(gboolean input_is_stream)
{
  LogProtoServer *proto;

  proto_server_options.max_msg_size = 32;

  proto = log_proto_indented_multiline_server_new(
            /* 32 bytes max line length */
            (input_is_stream ? log_transport_mock_stream_new : log_transport_mock_records_new)
            (
             "01234567\n", -1,
             "01234567\n", -1,
             "newline\n", -1,
             LTM_PADDING,
             LTM_EOF),
            get_inited_proto_server_options());

  assert_proto_server_fetch(proto, "01234567", -1);
  assert_proto_server_fetch(proto, "01234567", -1);

  log_proto_server_free(proto);
}

static void
test_input_starts_with_continuation(gboolean input_is_stream)
{
  LogProtoServer *proto;

  proto_server_options.max_msg_size = 32;

  proto = log_proto_indented_multiline_server_new(
            /* 32 bytes max line length */
            (input_is_stream ? log_transport_mock_stream_new : log_transport_mock_records_new)
            (
             " 01234567\n", -1,
             "01234567\n", -1,
             "newline\n", -1,
             LTM_PADDING,
             LTM_EOF),
            get_inited_proto_server_options());

  assert_proto_server_fetch(proto, " 01234567", -1);
  assert_proto_server_fetch(proto, "01234567", -1);

  log_proto_server_free(proto);
}

static void
test_multiline_at_eof(gboolean input_is_stream)
{
  LogProtoServer *proto;

  proto_server_options.max_msg_size = 32;

  proto = log_proto_indented_multiline_server_new(
            /* 32 bytes max line length */
            (input_is_stream ? log_transport_mock_stream_new : log_transport_mock_records_new)
            (
             "01234567\n", -1,
             " 01234567\n", -1,
             " end\n", -1,
             LTM_EOF),
            get_inited_proto_server_options());

  assert_proto_server_fetch(proto, "01234567\n"
                                   " 01234567\n"
                                   " end", -1);
  assert_proto_server_fetch_failure(proto, LPS_EOF, NULL);

  log_proto_server_free(proto);
}

void
test_log_proto_indented_multiline_server(void)
{
  PROTO_TESTCASE(test_proper_multiline, FALSE);
  PROTO_TESTCASE(test_proper_multiline, TRUE);
  PROTO_TESTCASE(test_line_without_continuation, FALSE);
  PROTO_TESTCASE(test_input_starts_with_continuation, TRUE);
  PROTO_TESTCASE(test_multiline_at_eof, FALSE);
  PROTO_TESTCASE(test_multiline_at_eof, TRUE);
}
