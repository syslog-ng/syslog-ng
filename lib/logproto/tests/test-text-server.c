#include "mock-transport.h"
#include "proto_lib.h"
#include "msg_parse_lib.h"
#include "logproto/logproto-text-server.h"

/****************************************************************************************
 * LogProtoTextServer
 ****************************************************************************************/

static void
test_log_proto_text_server_no_encoding(gboolean input_is_stream)
{
  LogProtoServer *proto;

  proto_server_options.max_msg_size = 32;
  proto = log_proto_text_server_new(
            /* 32 bytes max line length */
            (input_is_stream ? log_transport_mock_stream_new : log_transport_mock_records_new)(
              "01234567\n"
              /* line too long */
              "0123456789ABCDEF0123456789ABCDEF01234567\n"
              /* utf8 */
              "árvíztűrőtükörfúrógép\n"
              /* iso-8859-2 */
              "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa"      /*  |árvíztűrőtükörfú| */
              "\x72\xf3\x67\xe9\x70\n",                                               /*  |rógép|            */
              -1,

              /* NUL terminated line */
              "01234567\0"
              "01234567\0\n"
              "01234567\n\0"
              "01234567\r\n\0", 40,

              "01234567\r\n"
              /* no eol before EOF */
              "01234567", -1,

              LTM_EOF),
            get_inited_proto_server_options());
  assert_proto_server_fetch(proto, "01234567", -1);

  /* input split due to an oversized input line */
  assert_proto_server_fetch(proto, "0123456789ABCDEF0123456789ABCDEF", -1);
  assert_proto_server_fetch(proto, "01234567", -1);

  assert_proto_server_fetch(proto, "árvíztűrőtükörfúrógép", -1);
  assert_proto_server_fetch(proto, "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa"    /*  |árvíztűrőtükörfú| */
                            "\x72\xf3\x67\xe9\x70",                                               /*  |rógép|            */
                            -1);
  assert_proto_server_fetch(proto, "01234567", -1);

  assert_proto_server_fetch(proto, "01234567", -1);
  assert_proto_server_fetch(proto, "", -1);

  assert_proto_server_fetch(proto, "01234567", -1);
  assert_proto_server_fetch(proto, "", -1);

  assert_proto_server_fetch(proto, "01234567", -1);
  assert_proto_server_fetch(proto, "", -1);

  assert_proto_server_fetch(proto, "01234567", -1);
  assert_proto_server_fetch(proto, "01234567", -1);
  log_proto_server_free(proto);
}

static void
test_log_proto_text_server_no_eol_before_eof(void)
{
  LogProtoServer *proto;

  proto_server_options.max_msg_size = 32;
  proto = log_proto_text_server_new(
            log_transport_mock_stream_new(
              /* no eol before EOF */
              "01234567", -1,

              LTM_EOF),
           get_inited_proto_server_options());
  assert_proto_server_fetch(proto, "01234567", -1);
  assert_proto_server_fetch_failure(proto, LPS_EOF, NULL);
  log_proto_server_free(proto);

}

static void
test_log_proto_text_server_eol_before_eof(void)
{
  LogProtoServer *proto;

  proto_server_options.max_msg_size = 32;
  proto = log_proto_text_server_new(
            log_transport_mock_records_new(
              /* eol before EOF */
              "01234\n567\n890\n", -1,
              LTM_INJECT_ERROR(EIO),
              LTM_EOF),
           get_inited_proto_server_options());
  assert_proto_server_fetch(proto, "01234", -1);
  assert_proto_server_fetch(proto, "567", -1);
  assert_proto_server_fetch(proto, "890", -1);
  assert_proto_server_fetch_failure(proto, LPS_ERROR, NULL);
}

static void
test_log_proto_text_server_io_error_before_eof(void)
{
  LogProtoServer *proto;

  proto_server_options.max_msg_size = 32;
  proto = log_proto_text_server_new(
            log_transport_mock_stream_new(
              "01234567", -1,
              LTM_INJECT_ERROR(EIO),
              LTM_EOF),
            get_inited_proto_server_options());
  assert_proto_server_fetch(proto, "01234567", -1);
  assert_proto_server_fetch_failure(proto, LPS_ERROR, NULL);
  log_proto_server_free(proto);
}

static void
test_log_proto_text_server_partial_chars_before_eof(void)
{
  LogProtoServer *proto;

  proto_server_options.max_msg_size = 32;
  log_proto_server_options_set_encoding(&proto_server_options, "utf-8");
  proto = log_proto_text_server_new(
            log_transport_mock_stream_new(
              /* utf8 */
              "\xc3", -1,
              LTM_EOF),
            get_inited_proto_server_options());
  assert_proto_server_fetch_failure(proto, LPS_EOF, "EOF read on a channel with leftovers from previous character conversion, dropping input");
  log_proto_server_free(proto);
}

static void
test_log_proto_text_server_not_fixed_encoding(void)
{
  LogProtoServer *proto;

  proto_server_options.max_msg_size = 32;
  log_proto_server_options_set_encoding(&proto_server_options, "utf-8");
  /* to test whether a non-easily-reversable charset works too */
  proto = log_proto_text_server_new(
            log_transport_mock_stream_new(
              /* utf8 */
              "árvíztűrőtükörfúrógép\n", -1,
              LTM_EOF),
            get_inited_proto_server_options());
  assert_proto_server_fetch(proto, "árvíztűrőtükörfúrógép", -1);
  assert_proto_server_fetch_failure(proto, LPS_EOF, NULL);
  log_proto_server_free(proto);
}

static void
test_log_proto_text_server_ucs4(void)
{
  LogProtoServer *proto;

  proto_server_options.max_msg_size = 32;
  log_proto_server_options_set_encoding(&proto_server_options, "ucs-4");
  proto = log_proto_text_server_new(
            log_transport_mock_stream_new(
              /* ucs4 */
              "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"      /* |...á...r...v...í| */
              "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\x72"      /* |...z...t...ű...r| */
              "\x00\x00\x01\x51\x00\x00\x00\x74\x00\x00\x00\xfc\x00\x00\x00\x6b"      /* |...Q...t.......k| */
              "\x00\x00\x00\xf6\x00\x00\x00\x72\x00\x00\x00\x66\x00\x00\x00\xfa"      /* |.......r...f....| */
              "\x00\x00\x00\x72\x00\x00\x00\xf3\x00\x00\x00\x67\x00\x00\x00\xe9"      /* |...r.......g....| */
              "\x00\x00\x00\x70\x00\x00\x00\x0a", 88,                                 /* |...p....|         */
              LTM_EOF),
            get_inited_proto_server_options());
  assert_proto_server_fetch(proto, "árvíztűrőtükörfúrógép", -1);
  assert_proto_server_fetch_failure(proto, LPS_EOF, NULL);
  log_proto_server_free(proto);
}

static void
test_log_proto_text_server_iso8859_2(void)
{
  LogProtoServer *proto;

  proto_server_options.max_msg_size = 32;
  log_proto_server_options_set_encoding(&proto_server_options, "iso-8859-2");
  proto = log_proto_text_server_new(
            log_transport_mock_stream_new(
              /* iso-8859-2 */
              "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa"      /*  |árvíztűrőtükörfú| */
              "\x72\xf3\x67\xe9\x70\n", -1,                                           /*  |rógép|            */
              LTM_EOF),
            get_inited_proto_server_options());
  assert_proto_server_fetch(proto, "árvíztűrőtükörfúrógép", -1);
  assert_proto_server_fetch_failure(proto, LPS_EOF, NULL);
  log_proto_server_free(proto);
}

static void
test_log_proto_text_server_multi_read(void)
{
  LogProtoServer *proto;

  proto_server_options.max_msg_size = 32;
  proto = log_proto_text_server_new(
            log_transport_mock_records_new(
              "foobar\n", -1,
              /* no EOL, proto implementation would read another chunk */
              "foobaz", -1,
              LTM_INJECT_ERROR(EIO),
              LTM_EOF),
            get_inited_proto_server_options());
  assert_proto_server_fetch(proto, "foobar", -1);
  assert_proto_server_fetch(proto, "foobaz", -1);
  assert_proto_server_fetch_failure(proto, LPS_ERROR, NULL);
  log_proto_server_free(proto);
}

static void
test_log_proto_text_server_multi_read_not_allowed(void)
{
  /* FIXME: */
#if 0
  LogProtoServer *proto;

  proto_server_options.max_msg_size = 32;
  proto = log_proto_text_server_new(
            log_transport_mock_records_new(
              "foobar\n", -1,
              /* no EOL, proto implementation would read another chunk */
              "foobaz", -1,
              LTM_INJECT_ERROR(EIO),
              LTM_EOF),
            get_inited_proto_server_options());
  ((LogProtoBufferedServer *) proto)->no_multi_read = TRUE;
  assert_proto_server_fetch_single_read(proto, "foobar", -1);
  /* because of EAGAIN */
  assert_proto_server_fetch_single_read(proto, NULL, -1);
  /* because of NOMREAD, partial lines are returned as empty */
  assert_proto_server_fetch_single_read(proto, NULL, -1);
  /* because of EAGAIN */
  assert_proto_server_fetch_single_read(proto, NULL, -1);
  /* error was detected by this time, partial line is returned before the error */
  assert_proto_server_fetch_single_read(proto, "foobaz", -1);
  /* finally the error is returned too */
  assert_proto_server_fetch_failure(proto, LPS_ERROR, NULL);
  log_proto_server_free(proto);
#endif

}

void
test_log_proto_text_server(void)
{
  PROTO_TESTCASE(test_log_proto_text_server_no_encoding, FALSE);
  PROTO_TESTCASE(test_log_proto_text_server_no_encoding, TRUE);
  PROTO_TESTCASE(test_log_proto_text_server_no_eol_before_eof);
  PROTO_TESTCASE(test_log_proto_text_server_eol_before_eof);
  PROTO_TESTCASE(test_log_proto_text_server_io_error_before_eof);
  PROTO_TESTCASE(test_log_proto_text_server_partial_chars_before_eof);
  PROTO_TESTCASE(test_log_proto_text_server_not_fixed_encoding);
  PROTO_TESTCASE(test_log_proto_text_server_ucs4);
  PROTO_TESTCASE(test_log_proto_text_server_iso8859_2);
  PROTO_TESTCASE(test_log_proto_text_server_multi_read);
  PROTO_TESTCASE(test_log_proto_text_server_multi_read_not_allowed);
}
