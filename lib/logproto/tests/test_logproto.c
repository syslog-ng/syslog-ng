#include "mock-transport.h"
#include "proto_lib.h"
#include "msg_parse_lib.h"
#include "logproto/logproto-text-server.h"
#include "logproto/logproto-framed-server.h"
#include "logproto/logproto-dgram-server.h"
#include "logproto/logproto-record-server.h"

#include "apphook.h"

static void
test_log_proto_base(void)
{
  LogProtoServer *proto;

  assert_gint(log_proto_get_char_size_for_fixed_encoding("iso-8859-2"), 1, NULL);
  assert_gint(log_proto_get_char_size_for_fixed_encoding("ucs-4"), 4, NULL);

  log_proto_server_options_set_encoding(&proto_server_options, "ucs-4");
  proto = log_proto_binary_record_server_new(
            log_transport_mock_records_new(
              /* ucs4, terminated by record size */
              "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"      /* |...á...r...v...í| */
              "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\x72", 32, /* |...z...t...ű...r|  */
              LTM_EOF),
            get_inited_proto_server_options(), 32);

  /* check if error state is not forgotten unless reset_error is called */
  proto->status = LPS_ERROR;
  assert_proto_server_status(proto, proto->status , LPS_ERROR);
  assert_proto_server_fetch_failure(proto, LPS_ERROR, NULL);

  log_proto_server_reset_error(proto);
  assert_proto_server_fetch(proto, "árvíztűr", -1);
  assert_proto_server_status(proto, proto->status, LPS_SUCCESS);

  log_proto_server_free(proto);
}

/****************************************************************************************
 * LogProtoFramedServer
 ****************************************************************************************/
static void
test_log_proto_framed_server_simple_messages(void)
{
  LogProtoServer *proto;

  proto_server_options.max_msg_size = 32;
  proto = log_proto_framed_server_new(
            log_transport_mock_stream_new(
              "32 0123456789ABCDEF0123456789ABCDEF", -1,
              "10 01234567\n\n", -1,
              "10 01234567\0\0", 13,
              /* utf8 */
              "30 árvíztűrőtükörfúrógép", -1,
              /* iso-8859-2 */
              "21 \xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa"      /*  |árvíztűrőtükörfú| */
              "\x72\xf3\x67\xe9\x70", -1,                                                /*  |rógép|            */
              /* ucs4 */
              "32 \x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"      /* |...á...r...v...í| */
              "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\x72", 35,    /* |...z...t...ű...r|  */
              LTM_EOF),
            get_inited_proto_server_options());
  assert_proto_server_fetch(proto, "0123456789ABCDEF0123456789ABCDEF", -1);
  assert_proto_server_fetch(proto, "01234567\n\n", -1);
  assert_proto_server_fetch(proto, "01234567\0\0", 10);
  assert_proto_server_fetch(proto, "árvíztűrőtükörfúrógép", -1);
  assert_proto_server_fetch(proto, "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa"        /*  |.rv.zt.r.t.k.rf.| */
                            "\x72\xf3\x67\xe9\x70", -1);                                              /*  |r.g.p|            */
  assert_proto_server_fetch(proto, "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"        /* |...á...r...v...í| */
                            "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\x72", 32);  /* |...z...t...q...r|  */
  assert_proto_server_fetch_failure(proto, LPS_EOF, NULL);
  log_proto_server_free(proto);
}

static void
test_log_proto_framed_server_io_error(void)
{
  LogProtoServer *proto;

  proto_server_options.max_msg_size = 32;
  proto = log_proto_framed_server_new(
            log_transport_mock_stream_new(
              "32 0123456789ABCDEF0123456789ABCDEF", -1,
              LTM_INJECT_ERROR(EIO),
              LTM_EOF),
            get_inited_proto_server_options());
  assert_proto_server_fetch(proto, "0123456789ABCDEF0123456789ABCDEF", -1);
  assert_proto_server_fetch_failure(proto, LPS_ERROR, "Error reading RFC5428 style framed data");
  log_proto_server_free(proto);
}


static void
test_log_proto_framed_server_invalid_header(void)
{
  LogProtoServer *proto;

  proto_server_options.max_msg_size = 32;
  proto = log_proto_framed_server_new(
            log_transport_mock_stream_new(
              "1q 0123456789ABCDEF0123456789ABCDEF", -1,
              LTM_EOF),
            get_inited_proto_server_options());
  assert_proto_server_fetch_failure(proto, LPS_ERROR, "Invalid frame header");
  log_proto_server_free(proto);
}

static void
test_log_proto_framed_server_too_long_line(void)
{
  LogProtoServer *proto;

  proto_server_options.max_msg_size = 32;
  proto = log_proto_framed_server_new(
            log_transport_mock_stream_new(
              "48 0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF", -1,
              LTM_EOF),
            get_inited_proto_server_options());
  assert_proto_server_fetch_failure(proto, LPS_ERROR, "Incoming frame larger than log_msg_size()");
  log_proto_server_free(proto);
}

static void
test_log_proto_framed_server_message_exceeds_buffer(void)
{
  LogProtoServer *proto;

  /* should cause the buffer to be extended, shifted, as the first message
   * resizes the buffer to 16+10 == 26 bytes. */

  proto_server_options.max_msg_size = 32;
  proto = log_proto_framed_server_new(
            log_transport_mock_records_new(
              "16 0123456789ABCDE\n16 0123456789ABCDE\n", -1,
              LTM_EOF),
            get_inited_proto_server_options());
  assert_proto_server_fetch(proto, "0123456789ABCDE\n", -1);
  assert_proto_server_fetch(proto, "0123456789ABCDE\n", -1);
  log_proto_server_free(proto);
}

static void
test_log_proto_framed_server_buffer_shift_before_fetch(void)
{
  LogProtoServer *proto;

  /* this testcase fills the initially 10 byte buffer with data, which
   * causes a shift in log_proto_framed_server_fetch() */
  proto_server_options.max_msg_size = 32;
  proto_server_options.init_buffer_size = 10;
  proto_server_options.max_buffer_size = 10;
  proto = log_proto_framed_server_new(
            log_transport_mock_records_new(
              "7 012345\n4", 10,
              " 123\n", -1,
              LTM_EOF),
            get_inited_proto_server_options());
  assert_proto_server_fetch(proto, "012345\n", -1);
  assert_proto_server_fetch(proto, "123\n", -1);
  log_proto_server_free(proto);
}

static void
test_log_proto_framed_server_buffer_shift_to_make_space_for_a_frame(void)
{
  LogProtoServer *proto;

  /* this testcase fills the initially 10 byte buffer with data, which
   * causes a shift in log_proto_framed_server_fetch() */
  proto_server_options.max_msg_size = 32;
  proto_server_options.init_buffer_size = 10;
  proto_server_options.max_buffer_size = 10;
  proto = log_proto_framed_server_new(
            log_transport_mock_records_new(
              "6 01234\n4 ", 10,
              "123\n", -1,
              LTM_EOF),
            get_inited_proto_server_options());
  assert_proto_server_fetch(proto, "01234\n", -1);
  assert_proto_server_fetch(proto, "123\n", -1);
  log_proto_server_free(proto);
}

static void
test_log_proto_framed_server_multi_read(void)
{
  LogProtoServer *proto;

  proto_server_options.max_msg_size = 32;
  proto = log_proto_framed_server_new(
            log_transport_mock_records_new(
              "7 foobar\n", -1,
              /* no EOL, proto implementation would read another chunk */
              "6 fooba", -1,
              LTM_INJECT_ERROR(EIO),
              LTM_EOF),
            get_inited_proto_server_options());
  assert_proto_server_fetch(proto, "foobar\n", -1);
  /* with multi-read, we get the injected failure at the 2nd fetch */
  assert_proto_server_fetch_failure(proto, LPS_ERROR, "Error reading RFC5428 style framed data");
  log_proto_server_free(proto);

  /* NOTE: LPBS_NOMREAD is not implemented for framed protocol */
}

static void
test_log_proto_framed_server(void)
{
  PROTO_TESTCASE(test_log_proto_framed_server_simple_messages);
  PROTO_TESTCASE(test_log_proto_framed_server_io_error);
  PROTO_TESTCASE(test_log_proto_framed_server_invalid_header);
  PROTO_TESTCASE(test_log_proto_framed_server_too_long_line);
  PROTO_TESTCASE(test_log_proto_framed_server_message_exceeds_buffer);
  PROTO_TESTCASE(test_log_proto_framed_server_buffer_shift_before_fetch);
  PROTO_TESTCASE(test_log_proto_framed_server_buffer_shift_to_make_space_for_a_frame);
  PROTO_TESTCASE(test_log_proto_framed_server_multi_read);
}

static void
test_log_proto(void)
{
  /*
   * Things that are yet to be done:
   *
   * log_proto_text_server_new
   *   - apply-state/restart_with_state
   *     - questions: maybe move this to a separate LogProtoFileReader?
   *     - apply state:
   *       - same file, continued: same inode, size grown,
   *       - truncated file: same inode, size smaller
   *          - file starts over, all state data is reset!
   *        - buffer:
   *          - no encoding
   *          - encoding: utf8, ucs4, koi8r
   *        - state version: v1, v2, v3, v4
   *    - queued
   *    - saddr caching
   *
   * log_proto_text_client_new
   * log_proto_file_writer_new
   * log_proto_framed_client_new
   */
  test_log_proto_server_options();
  test_log_proto_base();
  test_log_proto_record_server();
  test_log_proto_text_server();
  test_log_proto_dgram_server();
  test_log_proto_framed_server();
}


int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  app_startup();

  init_proto_tests();

  test_log_proto();

  deinit_proto_tests();
  app_shutdown();
  return 0;
}
