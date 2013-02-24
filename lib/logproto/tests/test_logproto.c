#include "mock-transport.h"
#include "proto_lib.h"
#include "msg_parse_lib.h"
#include "logproto/logproto-text-server.h"
#include "logproto/logproto-framed-server.h"
#include "logproto/logproto-dgram-server.h"
#include "logproto/logproto-record-server.h"

#include "apphook.h"

static void
test_log_proto_server_options_limits(void)
{
  LogProtoServerOptions opts;

  log_proto_testcase_begin("test_log_proto_server_options_limits");
  log_proto_server_options_defaults(&opts);
  log_proto_server_options_init(&opts, configuration);
  assert_true(opts.max_msg_size > 0, "LogProtoServerOptions.max_msg_size is not initialized properly, max_msg_size=%d", opts.max_msg_size);
  assert_true(opts.init_buffer_size > 0, "LogProtoServerOptions.init_buffer_size is not initialized properly, init_buffer_size=%d", opts.init_buffer_size);
  assert_true(opts.max_buffer_size > 0, "LogProtoServerOptions.max_buffer_size is not initialized properly, max_buffer_size=%d", opts.max_buffer_size);

  assert_true(log_proto_server_options_validate(&opts), "Default LogProtoServerOptions were deemed invalid");
  log_proto_server_options_destroy(&opts);
  log_proto_testcase_end();
}

static void
test_log_proto_server_options_valid_encoding(void)
{
  LogProtoServerOptions opts;

  log_proto_testcase_begin("test_log_proto_server_options_valid_encoding");
  log_proto_server_options_defaults(&opts);
  /* check that encoding can be set and error is properly returned */
  log_proto_server_options_set_encoding(&opts, "utf-8");
  assert_string(opts.encoding, "utf-8", "LogProtoServerOptions.encoding was not properly set");

  log_proto_server_options_init(&opts, configuration);
  assert_true(log_proto_server_options_validate(&opts), "utf-8 was not accepted as a valid LogProtoServerOptions");
  log_proto_server_options_destroy(&opts);
  log_proto_testcase_end();
}

static void
test_log_proto_server_options_invalid_encoding(void)
{
  LogProtoServerOptions opts;
  gboolean success;

  log_proto_testcase_begin("test_log_proto_server_options_invalid_encoding");
  log_proto_server_options_defaults(&opts);

  log_proto_server_options_set_encoding(&opts, "never-ever-is-going-to-be-such-an-encoding");
  assert_string(opts.encoding, "never-ever-is-going-to-be-such-an-encoding", "LogProtoServerOptions.encoding was not properly set");

  log_proto_server_options_init(&opts, configuration);
  start_grabbing_messages();
  success = log_proto_server_options_validate(&opts);
  assert_grabbed_messages_contain("Unknown character set name specified; encoding='never-ever-is-going-to-be-such-an-encoding'", "message about unknown charset missing");
  assert_false(success, "Successfully set a bogus encoding, which is insane");

  log_proto_server_options_destroy(&opts);
  log_proto_testcase_end();
}

/* abstract LogProtoServer methods */
static void
test_log_proto_server_options(void)
{

  test_log_proto_server_options_limits();
  test_log_proto_server_options_valid_encoding();
  test_log_proto_server_options_invalid_encoding();
}

static void
test_log_proto_base(void)
{
  LogProtoServer *proto;

  log_proto_testcase_begin("test_log_proto_base");
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
  log_proto_testcase_end();
}

/****************************************************************************************
 * LogProtoRecordServer
 ****************************************************************************************/

static void
test_log_proto_binary_record_server_no_encoding(void)
{
  LogProtoServer *proto;

  log_proto_testcase_begin("test_log_proto_binary_record_server_no_encoding");
  proto = log_proto_binary_record_server_new(
            log_transport_mock_records_new(
              "0123456789ABCDEF0123456789ABCDEF", -1,
              "01234567\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n", -1,
              "01234567\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 32,
              /* utf8 */
              "árvíztűrőtükörfúrógép\n\n", 32,
              /* iso-8859-2 */
              "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa"      /*  |árvíztűrőtükörfú| */
              "\x72\xf3\x67\xe9\x70\n\n\n\n\n\n\n\n\n\n\n", -1,                       /*  |rógép|            */
              /* ucs4 */
              "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"      /* |...á...r...v...í| */
              "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\x72", 32, /* |...z...t...ű...r|  */

              "01234", 5,
              LTM_EOF),
            get_inited_proto_server_options(), 32);
  assert_proto_server_fetch(proto, "0123456789ABCDEF0123456789ABCDEF", -1);
  assert_proto_server_fetch(proto, "01234567\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n", -1);
  assert_proto_server_fetch(proto, "01234567\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 32);
  assert_proto_server_fetch(proto, "árvíztűrőtükörfúrógép\n\n", -1);
  assert_proto_server_fetch(proto, "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa"        /*  |.rv.zt.r.t.k.rf.| */
                            "\x72\xf3\x67\xe9\x70\n\n\n\n\n\n\n\n\n\n\n", -1);                        /*  |r.g.p|            */
  assert_proto_server_fetch(proto, "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"        /* |...á...r...v...í| */
                            "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\x72", 32);  /* |...z...t...q...r|  */
  assert_proto_server_fetch_failure(proto, LPS_ERROR, "Record size was set, and couldn't read enough bytes");
  log_proto_server_free(proto);
  log_proto_testcase_end();
}

static void
test_log_proto_padded_record_server_no_encoding(void)
{
  LogProtoServer *proto;

  log_proto_testcase_begin("test_log_proto_padded_record_server_no_encoding");
  proto = log_proto_padded_record_server_new(
            log_transport_mock_records_new(
              "0123456789ABCDEF0123456789ABCDEF", -1,
              "01234567\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n", -1,
              "01234567\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 32,
              /* utf8 */
              "árvíztűrőtükörfúrógép\n\n", 32,
              /* iso-8859-2 */
              "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa"      /*  |árvíztűrőtükörfú| */
              "\x72\xf3\x67\xe9\x70\n\n\n\n\n\n\n\n\n\n\n", -1,                       /*  |rógép|            */
              /* ucs4 */
              "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"      /* |...á...r...v...í| */
              "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\x72", 32, /* |...z...t...ű...r|  */

              "01234", 5,
              LTM_EOF),
            get_inited_proto_server_options(), 32);
  assert_proto_server_fetch(proto, "0123456789ABCDEF0123456789ABCDEF", -1);
  assert_proto_server_fetch(proto, "01234567", -1);
  assert_proto_server_fetch(proto, "01234567", -1);

  /* no encoding: utf8 remains utf8 */
  assert_proto_server_fetch(proto, "árvíztűrőtükörfúrógép", -1);

  /* no encoding: iso-8859-2 remains iso-8859-2 */
  assert_proto_server_fetch(proto, "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa" /*  |.rv.zt.r.t.k.rf.| */
                            "\x72\xf3\x67\xe9\x70",                                            /*  |r.g.p|            */
                            -1);
  /* no encoding, ucs4 becomes an empty string as it starts with a zero byte */
  assert_proto_server_fetch(proto, "", -1);
  assert_proto_server_fetch_failure(proto, LPS_ERROR, "Record size was set, and couldn't read enough bytes");
  log_proto_server_free(proto);
  log_proto_testcase_end();
}


static void
test_log_proto_padded_record_server_ucs4(void)
{
  LogProtoServer *proto;

  log_proto_testcase_begin("test_log_proto_padded_record_server_ucs4");
  log_proto_server_options_set_encoding(&proto_server_options, "ucs-4");
  proto = log_proto_padded_record_server_new(
            log_transport_mock_records_new(
              /* ucs4, terminated by record size */
              "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"      /* |...á...r...v...í| */
              "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\x72", 32, /* |...z...t...ű...r|  */

              /* ucs4, terminated by ucs4 encododed NL at the end */
              "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"      /* |...á...r...v...í| */
              "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\n", 32,   /* |...z...t...ű|  */

              "01234", 5,
              LTM_EOF),
            get_inited_proto_server_options(), 32);
  assert_proto_server_fetch(proto, "árvíztűr", -1);
  assert_proto_server_fetch(proto, "árvíztű", -1);
  assert_proto_server_fetch_failure(proto, LPS_ERROR, "Record size was set, and couldn't read enough bytes");
  log_proto_server_free(proto);
  log_proto_testcase_end();
}

static void
test_log_proto_padded_record_server_invalid_ucs4(void)
{
  LogProtoServer *proto;

  log_proto_testcase_begin("test_log_proto_padded_record_server_invalid_ucs4");
  log_proto_server_options_set_encoding(&proto_server_options, "ucs-4");
  proto = log_proto_padded_record_server_new(
            /* 31 bytes record size */
            log_transport_mock_records_new(
              /* invalid ucs4, trailing zeroes at the end */
              "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"      /* |...á...r...v...í| */
              "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00", 31, /* |...z...t...ű...r|  */
              LTM_EOF),
            get_inited_proto_server_options(), 31);
  assert_proto_server_fetch_failure(proto, LPS_ERROR, "Byte sequence too short, cannot convert an individual frame in its entirety");
  log_proto_server_free(proto);
  log_proto_testcase_end();
}

static void
test_log_proto_padded_record_server_iso_8859_2(void)
{
  LogProtoServer *proto;

  log_proto_testcase_begin("test_log_proto_padded_record_server_iso_8859_2");
  log_proto_server_options_set_encoding(&proto_server_options, "iso-8859-2");
  proto = log_proto_padded_record_server_new(
            /* 32 bytes record size */
            log_transport_mock_records_new(

              /* iso-8859-2, deliberately contains
               * accented chars so utf8 representation
               * becomes longer than the record size */
              "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa"       /*  |árvíztűrőtükörfú| */
              "\x72\xf3\x67\xe9\x70\xe9\xe9\xe9\xe9\xe9\xe9\xe9\xe9\xe9\xe9\xe9", -1,  /*  |rógépééééééééééé| */
              LTM_EOF),
            get_inited_proto_server_options(), 32);
  assert_proto_server_fetch(proto, "árvíztűrőtükörfúrógépééééééééééé", -1);
  assert_proto_server_fetch_failure(proto, LPS_EOF, NULL);
  log_proto_server_free(proto);
  log_proto_testcase_end();
}

static void
test_log_proto_record_server(void)
{
  /* binary records are only tested in no-encoding mode, as there's only one
   * differing code-path that is used in LPRS_BINARY mode */
  test_log_proto_binary_record_server_no_encoding();
  test_log_proto_padded_record_server_no_encoding();
  test_log_proto_padded_record_server_ucs4();
  test_log_proto_padded_record_server_invalid_ucs4();
  test_log_proto_padded_record_server_iso_8859_2();
}

/****************************************************************************************
 * LogProtoTextServer
 ****************************************************************************************/

static void
test_log_proto_text_server_no_encoding(gboolean input_is_stream)
{
  LogProtoServer *proto;

  log_proto_testcase_begin("test_log_proto_text_server_no_encoding");
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
  log_proto_testcase_end();
}

static void
test_log_proto_text_server_eof_handling(void)
{
  LogProtoServer *proto;

  log_proto_testcase_begin("test_log_proto_text_server_eof_handling(no eol before EOF)");
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
  log_proto_testcase_end();

  log_proto_testcase_begin("test_log_proto_text_server_eof_handling(I/O error before EOF)");
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
  log_proto_testcase_end();

  log_proto_testcase_begin("test_log_proto_text_server_eof_handling(partial characters)");
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
  log_proto_testcase_end();
}

static void
test_log_proto_text_server_not_fixed_encoding(void)
{
  LogProtoServer *proto;

  log_proto_testcase_begin("test_log_proto_text_server_not_fixed_encoding");
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
  log_proto_testcase_end();
}

static void
test_log_proto_text_server_ucs4(void)
{
  LogProtoServer *proto;

  log_proto_testcase_begin("test_log_proto_text_server_ucs4");
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
  log_proto_testcase_end();
}

static void
test_log_proto_text_server_iso8859_2(void)
{
  LogProtoServer *proto;

  log_proto_testcase_begin("test_log_proto_text_server_iso8859_2");
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
  log_proto_testcase_end();
}

static void
test_log_proto_text_server_multi_read(void)
{
  LogProtoServer *proto;

  log_proto_testcase_begin("test_log_proto_text_server_multi_read(multi-read-allowed)");
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
  log_proto_testcase_end();

#if 0
  log_proto_testcase_begin("test_log_proto_text_server_multi_read(multi-read-not-allowed)");
  proto_server_options.max_msg_size = 32;
  proto = construct_server_proto("no-multi-read-text",
            log_transport_mock_records_new(
              "foobar\n", -1,
              /* no EOL, proto implementation would read another chunk */
              "foobaz", -1,
              LTM_INJECT_ERROR(EIO),
              LTM_EOF),
            get_inited_proto_server_options());
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
  log_proto_testcase_end();
#endif
}

static void
test_log_proto_text_server(void)
{
  test_log_proto_text_server_no_encoding(FALSE);
  test_log_proto_text_server_no_encoding(TRUE);
  test_log_proto_text_server_eof_handling();
  test_log_proto_text_server_not_fixed_encoding();
  test_log_proto_text_server_ucs4();
  test_log_proto_text_server_iso8859_2();
  test_log_proto_text_server_multi_read();
}

/****************************************************************************************
 * LogProtoDGramServer
 ****************************************************************************************/

static void
test_log_proto_dgram_server_no_encoding(void)
{
  LogProtoServer *proto;

  log_proto_testcase_begin("test_log_proto_dgram_server_no_encoding");
  proto_server_options.max_msg_size = 32;
  proto = log_proto_dgram_server_new(
            log_transport_mock_endless_records_new(
              "0123456789ABCDEF0123456789ABCDEF", -1,
              "01234567\n", -1,
              "01234567\0", 9,
              /* utf8 */
              "árvíztűrőtükörfúrógép\n\n", 32,
              /* iso-8859-2 */
              "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa"      /*  |árvíztűrőtükörfú| */
              "\x72\xf3\x67\xe9\x70\n", -1,                                           /*  |rógép|            */
              /* ucs4 */
              "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"      /* |...á...r...v...í| */
              "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\x72", 32, /* |...z...t...ű...r|  */

              "01234", 5,
              LTM_EOF),
            get_inited_proto_server_options());
  assert_proto_server_fetch(proto, "0123456789ABCDEF0123456789ABCDEF", -1);
  assert_proto_server_fetch(proto, "01234567\n", -1);
  assert_proto_server_fetch(proto, "01234567\0", 9);

  /* no encoding: utf8 remains utf8 */
  assert_proto_server_fetch(proto, "árvíztűrőtükörfúrógép\n\n", -1);

  /* no encoding: iso-8859-2 remains iso-8859-2 */
  assert_proto_server_fetch(proto, "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa" /*  |.rv.zt.r.t.k.rf.| */
                            "\x72\xf3\x67\xe9\x70\n",                                          /*  |r.g.p|            */
                            -1);
  /* no encoding, ucs4 becomes a string with embedded NULs */
  assert_proto_server_fetch(proto, "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"       /* |...á...r...v...í| */
                            "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\x72", 32); /* |...z...t...ű...r|  */

  assert_proto_server_fetch(proto, "01234", -1);

  log_proto_server_free(proto);
  log_proto_testcase_end();
}


static void
test_log_proto_dgram_server_ucs4(void)
{
  LogProtoServer *proto;

  log_proto_testcase_begin("test_log_proto_dgram_server_ucs4");
  proto_server_options.max_msg_size = 32;
  log_proto_server_options_set_encoding(&proto_server_options, "ucs-4");
  proto = log_proto_dgram_server_new(
            log_transport_mock_endless_records_new(
              /* ucs4, terminated by record size */
              "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"      /* |...á...r...v...í| */
              "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\x72", 32, /* |...z...t...ű...r|  */

              /* ucs4, terminated by ucs4 encododed NL at the end */
              "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"      /* |...á...r...v...í| */
              "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\n", 32,   /* |...z...t...ű|  */

              LTM_EOF),
            get_inited_proto_server_options());
  assert_proto_server_fetch(proto, "árvíztűr", -1);
  assert_proto_server_fetch(proto, "árvíztű\n", -1);
  log_proto_server_free(proto);
  log_proto_testcase_end();
}

static void
test_log_proto_dgram_server_invalid_ucs4(void)
{
  LogProtoServer *proto;

  log_proto_testcase_begin("test_log_proto_dgram_server_invalid_ucs4");
  proto_server_options.max_msg_size = 32;
  log_proto_server_options_set_encoding(&proto_server_options, "ucs-4");
  proto = log_proto_dgram_server_new(
            /* 31 bytes record size */
            log_transport_mock_endless_records_new(
              /* invalid ucs4, trailing zeroes at the end */
              "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"      /* |...á...r...v...í| */
              "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00", 31, /* |...z...t...ű...r|  */
              LTM_EOF),
            get_inited_proto_server_options());
  assert_proto_server_fetch_failure(proto, LPS_ERROR, "Byte sequence too short, cannot convert an individual frame in its entirety");
  log_proto_server_free(proto);
  log_proto_testcase_end();
}

static void
test_log_proto_dgram_server_iso_8859_2(void)
{
  LogProtoServer *proto;

  log_proto_testcase_begin("test_log_proto_dgram_server_iso_8859_2");
  proto_server_options.max_msg_size = 32;
  log_proto_server_options_set_encoding(&proto_server_options, "iso-8859-2");
  proto = log_proto_dgram_server_new(
            log_transport_mock_endless_records_new(

              /* iso-8859-2, deliberately contains
               * accented chars so utf8 representation
               * becomes longer than the record size */
              "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa"       /*  |árvíztűrőtükörfú| */
              "\x72\xf3\x67\xe9\x70\xe9\xe9\xe9\xe9\xe9\xe9\xe9\xe9\xe9\xe9\xe9", -1,  /*  |rógépééééééééééé| */
              LTM_EOF),
            get_inited_proto_server_options());
  assert_proto_server_fetch(proto, "árvíztűrőtükörfúrógépééééééééééé", -1);
  assert_proto_server_fetch_ignored_eof(proto);
  log_proto_server_free(proto);
  log_proto_testcase_end();
}

static void
test_log_proto_dgram_server_eof_handling(void)
{
  LogProtoServer *proto;

  log_proto_testcase_begin("test_log_proto_dgram_server_eof_handling");
  proto_server_options.max_msg_size = 32;
  proto = log_proto_dgram_server_new(
            log_transport_mock_endless_records_new(
              /* no eol before EOF */
              "01234567", -1,

              LTM_EOF),
            get_inited_proto_server_options());
  assert_proto_server_fetch(proto, "01234567", -1);
  assert_proto_server_fetch_ignored_eof(proto);
  assert_proto_server_fetch_ignored_eof(proto);
  assert_proto_server_fetch_ignored_eof(proto);
  log_proto_server_free(proto);
  log_proto_testcase_end();
}

static void
test_log_proto_dgram_server(void)
{
  test_log_proto_dgram_server_no_encoding();
  test_log_proto_dgram_server_ucs4();
  test_log_proto_dgram_server_invalid_ucs4();
  test_log_proto_dgram_server_iso_8859_2();
  test_log_proto_dgram_server_eof_handling();
}

/****************************************************************************************
 * LogProtoFramedServer
 ****************************************************************************************/
static void
test_log_proto_framed_server_simple_messages(void)
{
  LogProtoServer *proto;

  log_proto_testcase_begin("test_log_proto_framed_server_simple_messages");
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
  log_proto_testcase_end();
}

static void
test_log_proto_framed_server_io_error(void)
{
  LogProtoServer *proto;

  log_proto_testcase_begin("test_log_proto_framed_server_io_error");
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
  log_proto_testcase_end();
}


static void
test_log_proto_framed_server_invalid_header(void)
{
  LogProtoServer *proto;

  log_proto_testcase_begin("test_log_proto_framed_server_invalid_header");
  proto_server_options.max_msg_size = 32;
  proto = log_proto_framed_server_new(
            log_transport_mock_stream_new(
              "1q 0123456789ABCDEF0123456789ABCDEF", -1,
              LTM_EOF),
            get_inited_proto_server_options());
  assert_proto_server_fetch_failure(proto, LPS_ERROR, "Invalid frame header");
  log_proto_server_free(proto);
  log_proto_testcase_end();
}

static void
test_log_proto_framed_server_too_long_line(void)
{
  LogProtoServer *proto;

  log_proto_testcase_begin("test_log_proto_framed_server_too_long_line");
  proto_server_options.max_msg_size = 32;
  proto = log_proto_framed_server_new(
            log_transport_mock_stream_new(
              "48 0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF", -1,
              LTM_EOF),
            get_inited_proto_server_options());
  assert_proto_server_fetch_failure(proto, LPS_ERROR, "Incoming frame larger than log_msg_size()");
  log_proto_server_free(proto);
  log_proto_testcase_end();
}

static void
test_log_proto_framed_server_message_exceeds_buffer(void)
{
  LogProtoServer *proto;

  /* should cause the buffer to be extended, shifted, as the first message
   * resizes the buffer to 16+10 == 26 bytes. */

  log_proto_testcase_begin("test_log_proto_framed_server_message_exceeds_buffer");
  proto_server_options.max_msg_size = 32;
  proto = log_proto_framed_server_new(
            log_transport_mock_records_new(
              "16 0123456789ABCDE\n16 0123456789ABCDE\n", -1,
              LTM_EOF),
            get_inited_proto_server_options());
  assert_proto_server_fetch(proto, "0123456789ABCDE\n", -1);
  assert_proto_server_fetch(proto, "0123456789ABCDE\n", -1);
  log_proto_server_free(proto);
  log_proto_testcase_end();
}

static void
test_log_proto_framed_server_buffer_shift_before_fetch(void)
{
  LogProtoServer *proto;

  log_proto_testcase_begin("test_log_proto_framed_server_buffer_shift_before_fetch");
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
  log_proto_testcase_end();
}

static void
test_log_proto_framed_server_buffer_shift_to_make_space_for_a_frame(void)
{
  LogProtoServer *proto;

  log_proto_testcase_begin("test_log_proto_framed_server_buffer_shift_to_make_space_for_a_frame");
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
  log_proto_testcase_end();
}

static void
test_log_proto_framed_server_multi_read(void)
{
  LogProtoServer *proto;

  log_proto_testcase_begin("test_log_proto_framed_server_multi_read");
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
  log_proto_testcase_end();
}

static void
test_log_proto_framed_server(void)
{
  test_log_proto_framed_server_simple_messages();
  test_log_proto_framed_server_io_error();
  test_log_proto_framed_server_invalid_header();
  test_log_proto_framed_server_too_long_line();
  test_log_proto_framed_server_message_exceeds_buffer();
  test_log_proto_framed_server_buffer_shift_before_fetch();
  test_log_proto_framed_server_buffer_shift_to_make_space_for_a_frame();
  test_log_proto_framed_server_multi_read();
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
