#include "mock-transport.h"
#include "logproto.h"
#include "msg_parse_lib.h"

#include "apphook.h"
#include "basic-proto.c"

static LogProto *create_log_proto_text_server_new(LogTransport *transport, gint size, guint flags);
static LogProtoStatus proto_fetch(LogProto *proto, const guchar **msg, gsize *msg_len);

#define assert_proto_status(status_param, expected_status_param) assert_gint(status_param, expected_status_param, "LogProto expected status mismatch at line: %i", __LINE__);

#define assert_proto_fetch(proto, expected_msg, expected_msg_len) \
      { \
        const guchar *msg = NULL; \
        gsize msg_len = 0; \
        LogProtoStatus status; \
        status = proto_fetch(proto, &msg, &msg_len); \
        assert_proto_status(status, LPS_SUCCESS); \
        assert_nstring((const gchar *) msg, msg_len, expected_msg, expected_msg_len, "LogProto expected message mismatch at line: %i", __LINE__); \
      }

#define assert_proto_fetch_single_read(proto, expected_msg, expected_msg_len) \
      { \
        const guchar *msg = NULL; \
        gsize msg_len = 0; \
        LogProtoStatus status; \
        GSockAddr *saddr = NULL; \
        gboolean may_read = TRUE; \
        start_grabbing_messages(); \
        status = log_proto_fetch(proto, &msg, &msg_len, &saddr, &may_read, FALSE); \
        assert_proto_status(status, LPS_SUCCESS); \
        if (expected_msg) \
          { \
            assert_nstring((const gchar *) msg, msg_len, expected_msg, expected_msg_len, "LogProto expected message mismatch at line: %i", __LINE__); \
          } \
        else \
          { \
            assert_true(msg == NULL, "when single-read finds an incomplete message, msg must be NULL at line: %i", __LINE__); \
            assert_true(saddr == NULL, "returned saddr must be NULL on success at line: %i", __LINE__); \
          } \
        stop_grabbing_messages(); \
      }

#define assert_proto_fetch_failure(proto, expected_status, error_message) \
      { \
        const guchar *msg = NULL; \
        gsize msg_len = 0; \
        LogProtoStatus status; \
        status = proto_fetch(proto, &msg, &msg_len); \
        assert_proto_status(status, expected_status); \
        if (error_message) \
          assert_grabbed_messages_contain(error_message, "expected error message didn't show up at line: %i", __LINE__); \
      }

#define assert_proto_fetch_ignored_eof(proto) \
      { \
        const guchar *msg = NULL; \
        gsize msg_len = 0; \
        LogProtoStatus status; \
        GSockAddr *saddr = NULL; \
        gboolean may_read = TRUE; \
        start_grabbing_messages(); \
        status = log_proto_fetch(proto, &msg, &msg_len, &saddr, &may_read, FALSE); \
        assert_proto_status(status, LPS_SUCCESS); \
        assert_true(msg == NULL, "when an EOF is ignored msg must be NULL at line: %i", __LINE__); \
        assert_true(saddr == NULL, "returned saddr must be NULL on success at line: %i", __LINE__); \
        stop_grabbing_messages(); \
      }

#define UTF8_BAD_INPUT_SIZE 49
/*                         H   ?   l   l   o   ?   |---á   ?   ?   |---ő   |---á   |---ű */
#define UTF8_BAD_INPUT "\x48\x83\x6C\x6C\x6F\xC3\xC3\xA1\xA9\xBA\xC5\x91\xC3\xA1\xC5\xB1" \
                       "\xC3\xBA\xC5\x91\xC3\xB3\xC3\xBC\xC3\xAD\xC3\x89\xC3\x81\x50\xC3" \
                       "\x9A\xC5\x90\xC3\x8D\xC3\x8D\xC3\x9C\xC3\x93\xC3\x9A\xC5\xB0\xA9" \
                       "\xA9"
#define UTF8_GOOD_OUTPUT "H?llo?á??őáűúőóüíÉÁPÚŐÍÍÜÓÚŰ??"

#define WINDOWS_1250_BAD_INPUT_SIZE 30
#define WINDOWS_1250_BAD_INPUT "\x48\x81\x83\x6C\x88\x20\x68\x61\x6C\x6C\xF3\x2C\x20\x65\x90\x65" \
                               "\x6B\x74\x6F\x72\x20\x6B\x61\x98\x61\x6E\x64\x6F\x72\x21"
#define WINDOWS_1250_GOOD_OUTPUT "H??l? halló, e?ektor ka?andor!"


#define WINDOWS_1252_BAD_INPUT_SIZE 30
#define WINDOWS_1252_BAD_INPUT "\x48\x81\x8D\x6C\x8F\x20\x68\x61\x6C\x6C\xF3\x2C\x20\x65\x90\x65" \
                               "\x6B\x74\x6F\x72\x20\x6B\x61\x9D\x61\x6E\x64\x6F\x72\x21"
#define WINDOWS_1252_GOOD_OUTPUT "H??l? halló, e?ektor ka?andor!"

#define UCS_2_BAD_INPUT_SIZE 60
#define UCS_2_BAD_INPUT "\x48\x00\x00\xD8\x7F\xDB\x6C\x00\x80\xDB\x20\x00\x68\x00\x61\x00" \
                        "\x6C\x00\x6C\x00\xF3\x00\x2C\x00\x20\x00\x65\x00\xFF\xDB\x65\x00" \
                        "\x6B\x00\x74\x00\x6F\x00\x72\x00\x20\x00\x6B\x00\x61\x00\xFF\xDF" \
                        "\x61\x00\x6E\x00\x64\x00\x6F\x00\x72\x00\x21\x00"
#define UCS_2_GOOD_OUTPUT "H??l? halló, e?ektor ka?andor!"

#define UTF16_BAD_INPUT_SIZE 60
#define UTF16_BAD_INPUT "\x48\x00\x65\x00\x00\xDC\x6C\x00\x6F\x00\x20\x00\x68\x00\x61\x00" \
                        "\xFF\xDF\x6C\x00\xF3\x00\x2C\x00\x20\x00\x65\x00\xFF\xDF\x65\x00" \
                        "\x6B\x00\x74\x00\x6F\x00\x72\x00\x20\x00\x6B\x00\x61\x00\xFF\xDC" \
                        "\x61\x00\x6E\x00\x64\x00\x6F\x00\x72\x00\x21\x00"
#define UTF16_GOOD_OUTPUT "He?lo ha?ló, e?ektor ka?andor!"

#define TEST_LOG_PROTO_TEXT_RECORD_SERVER_SKIP_BAD_CHARS(bad_input, bad_input_size, encoding, good_output) \
    { \
      LogProto *proto = log_proto_record_server_new(log_transport_mock_new(FALSE, bad_input, bad_input_size, LTM_EOF), bad_input_size, 0); \
      log_proto_set_encoding(proto, encoding); \
      assert_proto_fetch(proto, good_output, -1); \
      assert_proto_fetch_failure(proto, LPS_EOF, NULL); \
      log_proto_free(proto); \
    }

#define TEST_LOG_PROTO_TEXT_SERVER_SKIP_BAD_CHARS(bad_input, bad_input_size, encoding, good_output) \
    { \
      LogTransport *transport = log_transport_mock_new(TRUE, bad_input, bad_input_size, LTM_EOF); \
      LogProto *proto = create_log_proto_text_server_new(transport, 32, LPBS_POS_TRACKING); \
      log_proto_set_encoding(proto, encoding); \
      assert_proto_fetch(proto, good_output, -1); \
      assert_proto_fetch_failure(proto, LPS_EOF, NULL); \
      log_proto_free(proto); \
    }

#define TEST_LOG_PROTO_DGRAM_SERVER_SKIP_BAD_CHARS(bad_input, bad_input_size, encoding, good_output) \
    { \
      LogProto *proto = log_proto_dgram_server_new(log_transport_mock_new(FALSE, bad_input, bad_input_size, LTM_EOF), bad_input_size, 0); \
      log_proto_set_encoding(proto, encoding); \
      assert_proto_fetch(proto, good_output, -1); \
      assert_proto_fetch_ignored_eof(proto); \
      log_proto_free(proto); \
    }

static LogProto *
create_log_proto_text_server_new(LogTransport *transport, gint size, guint flags)
{
  LogProto *proto;
  LogProtoServerOptions *proto_options;
  proto_options = g_malloc0(sizeof(LogProtoServerOptions));
  proto_options->super.size = size;
  proto_options->super.flags = flags;

  proto = log_proto_text_server_new(transport, proto_options);
  g_free(proto_options);
  return proto;
}

static LogProtoStatus
proto_fetch(LogProto *proto, const guchar **msg, gsize *msg_len)
{
  gboolean may_read = TRUE;
  LogProtoStatus status;

  start_grabbing_messages();
  do
    {
      /* FIXME: pass pointer to GSockAddr: without later memory corruption */
      status = log_proto_fetch(proto, msg, msg_len, NULL, &may_read, FALSE);
    }
  while (status == LPS_SUCCESS && *msg == NULL);

  stop_grabbing_messages();
  return status;
}

/* abstract LogProto methods */
static void
test_log_proto_base(void)
{
  LogProto *proto;

  assert_gint(log_proto_get_char_size_for_fixed_encoding("iso-8859-2"), 1, NULL);
  assert_gint(log_proto_get_char_size_for_fixed_encoding("ucs-4"), 4, NULL);

  proto = log_proto_record_server_new(
            log_transport_mock_new(
              FALSE,
              /* ucs4, terminated by record size */
              "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"      /* |...á...r...v...í| */
              "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\x72", 32, /* |...z...t...ű...r|  */
              LTM_EOF),
            32, 0);

  /* check that encoding can be set and error is properly returned */
  assert_true(log_proto_set_encoding(proto, "utf-8"), "Error setting encoding to utf8");
  assert_string(proto->encoding, "utf-8", NULL);

  assert_false(log_proto_set_encoding(proto, "never-ever-is-going-to-be-such-an-encoding"), "Successfully set a bogus encoding, which is insane");
  assert_true(proto->encoding == NULL, "an failed log_proto_set_encoding call left the encoding lingering");

  log_proto_set_encoding(proto, "ucs-4");
  assert_string(proto->encoding, "ucs-4", NULL);
  assert_proto_fetch(proto, "árvíztűr", -1);
  log_proto_free(proto);
}

/****************************************************************************************
 * LogProtoRecordServer
 ****************************************************************************************/

static void
test_log_proto_binary_record_server_no_encoding(void)
{
  LogProto *proto;

  proto = log_proto_record_server_new(
            log_transport_mock_new(
              FALSE,
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
            32,
            LPRS_BINARY);
  assert_proto_fetch(proto, "0123456789ABCDEF0123456789ABCDEF", -1);
  assert_proto_fetch(proto, "01234567\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n", -1);
  assert_proto_fetch(proto, "01234567\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 32);
  assert_proto_fetch(proto, "árvíztűrőtükörfúrógép\n\n", -1);
  assert_proto_fetch(proto, "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa"        /*  |.rv.zt.r.t.k.rf.| */
                            "\x72\xf3\x67\xe9\x70\n\n\n\n\n\n\n\n\n\n\n", -1);                        /*  |r.g.p|            */
  assert_proto_fetch(proto, "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"        /* |...á...r...v...í| */
                            "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\x72", 32);  /* |...z...t...q...r|  */
  assert_proto_fetch_failure(proto, LPS_ERROR, "Padding was set, and couldn't read enough bytes");
  log_proto_free(proto);
}

static void
test_log_proto_text_record_server_no_encoding(void)
{
  LogProto *proto;

  proto = log_proto_record_server_new(
            log_transport_mock_new(
              FALSE,
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
            32, 0);
  assert_proto_fetch(proto, "0123456789ABCDEF0123456789ABCDEF", -1);
  assert_proto_fetch(proto, "01234567", -1);
  assert_proto_fetch(proto, "01234567", -1);

  /* no encoding: utf8 remains utf8 */
  assert_proto_fetch(proto, "árvíztűrőtükörfúrógép", -1);

  /* no encoding: iso-8859-2 remains iso-8859-2 */
  assert_proto_fetch(proto, "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa" /*  |.rv.zt.r.t.k.rf.| */
                            "\x72\xf3\x67\xe9\x70",                                            /*  |r.g.p|            */
                            -1);
  /* no encoding, ucs4 becomes an empty string as it starts with a zero byte */
  assert_proto_fetch(proto, "", -1);
  assert_proto_fetch_failure(proto, LPS_ERROR, "Padding was set, and couldn't read enough bytes");
  log_proto_free(proto);
}


static void
test_log_proto_text_record_server_ucs4(void)
{
  LogProto *proto;

  proto = log_proto_record_server_new(
            log_transport_mock_new(
              FALSE,
              /* ucs4, terminated by record size */
              "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"      /* |...á...r...v...í| */
              "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\x72", 32, /* |...z...t...ű...r|  */

              /* ucs4, terminated by ucs4 encododed NL at the end */
              "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"      /* |...á...r...v...í| */
              "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\n", 32,   /* |...z...t...ű|  */

              "01234", 5,
              LTM_EOF),
            32, 0);
  log_proto_set_encoding(proto, "ucs-4");
  assert_proto_fetch(proto, "árvíztűr", -1);
  assert_proto_fetch(proto, "árvíztű", -1);
  assert_proto_fetch_failure(proto, LPS_ERROR, "Padding was set, and couldn't read enough bytes");
  log_proto_free(proto);
}

static void
test_log_proto_text_record_server_iso_8859_2(void)
{
  LogProto *proto;

  proto = log_proto_record_server_new(
            /* 32 bytes record size */
            log_transport_mock_new(
              FALSE,

              /* iso-8859-2, deliberately contains
               * accented chars so utf8 representation
               * becomes longer than the record size */
              "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa"       /*  |árvíztűrőtükörfú| */
              "\x72\xf3\x67\xe9\x70\xe9\xe9\xe9\xe9\xe9\xe9\xe9\xe9\xe9\xe9\xe9", -1,  /*  |rógépééééééééééé| */
              LTM_EOF),
            32, 0);
  log_proto_set_encoding(proto, "iso-8859-2");
  assert_proto_fetch(proto, "árvíztűrőtükörfúrógépééééééééééé", -1);
  assert_proto_fetch_failure(proto, LPS_EOF, NULL);
  log_proto_free(proto);
}

static void
test_log_proto_text_record_server_skip_bad_chars(void)
{
  TEST_LOG_PROTO_TEXT_RECORD_SERVER_SKIP_BAD_CHARS(UTF8_BAD_INPUT, UTF8_BAD_INPUT_SIZE, "utf-8", UTF8_GOOD_OUTPUT);
  TEST_LOG_PROTO_TEXT_RECORD_SERVER_SKIP_BAD_CHARS(WINDOWS_1250_BAD_INPUT, WINDOWS_1250_BAD_INPUT_SIZE, "windows-1250", WINDOWS_1250_GOOD_OUTPUT);
  TEST_LOG_PROTO_TEXT_RECORD_SERVER_SKIP_BAD_CHARS(WINDOWS_1252_BAD_INPUT, WINDOWS_1252_BAD_INPUT_SIZE, "windows-1252", WINDOWS_1252_GOOD_OUTPUT);
  TEST_LOG_PROTO_TEXT_RECORD_SERVER_SKIP_BAD_CHARS(UCS_2_BAD_INPUT, UCS_2_BAD_INPUT_SIZE, "ucs-2le", UCS_2_GOOD_OUTPUT);
  TEST_LOG_PROTO_TEXT_RECORD_SERVER_SKIP_BAD_CHARS(UTF16_BAD_INPUT, UTF16_BAD_INPUT_SIZE, "utf-16le", UTF16_GOOD_OUTPUT);
}

static void
test_log_proto_record_server(void)
{
  /* binary records are only tested in no-encoding mode, as there's only one
   * differing code-path that is used in LPRS_BINARY mode */
  test_log_proto_binary_record_server_no_encoding();
  test_log_proto_text_record_server_no_encoding();
  test_log_proto_text_record_server_ucs4();
  test_log_proto_text_record_server_iso_8859_2();
  test_log_proto_text_record_server_skip_bad_chars();
}

/****************************************************************************************
 * LogProtoTextServer
 ****************************************************************************************/

static void
test_log_proto_text_server_no_encoding(gboolean input_is_stream)
{
  LogProto *proto;
  LogTransport *transport = log_transport_mock_new(
      input_is_stream,
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

      LTM_EOF);

  /* 32 bytes max line length */
  proto = create_log_proto_text_server_new(transport, 32, LPBS_POS_TRACKING);
  assert_proto_fetch(proto, "01234567", -1);

  /* input split due to an oversized input line */
  assert_proto_fetch(proto, "0123456789ABCDEF0123456789ABCDEF", -1);
  assert_proto_fetch(proto, "01234567", -1);

  assert_proto_fetch(proto, "árvíztűrőtükörfúrógép", -1);
  assert_proto_fetch(proto, "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa"    /*  |árvíztűrőtükörfú| */
                            "\x72\xf3\x67\xe9\x70",                                               /*  |rógép|            */
                            -1);
  assert_proto_fetch(proto, "01234567", -1);

  assert_proto_fetch(proto, "01234567", -1);
  assert_proto_fetch(proto, "", -1);

  assert_proto_fetch(proto, "01234567", -1);
  assert_proto_fetch(proto, "", -1);

  assert_proto_fetch(proto, "01234567", -1);
  assert_proto_fetch(proto, "", -1);

  assert_proto_fetch(proto, "01234567", -1);
  assert_proto_fetch(proto, "01234567", -1);
  log_proto_free(proto);
}

static void
test_log_proto_text_server_eof_handling(void)
{
  LogProto *proto;
  LogTransport *transport;

  transport = log_transport_mock_new(
      TRUE,
      /* no eol before EOF */
      "01234567", -1,
      LTM_EOF);
  proto = create_log_proto_text_server_new(transport, 32, LPBS_POS_TRACKING);
  assert_proto_fetch(proto, "01234567", -1);
  assert_proto_fetch_failure(proto, LPS_EOF, NULL);
  log_proto_free(proto);

  transport = log_transport_mock_new(
      TRUE,
      "01234567", -1,
      LTM_INJECT_ERROR(EIO),
      LTM_EOF);
  proto = create_log_proto_text_server_new(transport, 32, LPBS_POS_TRACKING);
  assert_proto_fetch(proto, "01234567", -1);
  assert_proto_fetch_failure(proto, LPS_ERROR, NULL);
  log_proto_free(proto);

  transport = log_transport_mock_new(
      TRUE,
      /* utf8 */
      "\xc3", -1,
      LTM_EOF);
  proto = create_log_proto_text_server_new(transport, 32, LPBS_POS_TRACKING);
  log_proto_set_encoding(proto, "utf-8");
  assert_proto_fetch_failure(proto, LPS_EOF, "EOF read on a channel with leftovers from previous character conversion, dropping input");
  log_proto_free(proto);
}

static void
test_log_proto_text_server_not_fixed_encoding(void)
{
  /* to test whether a non-easily-reversable charset works too */

  LogProto *proto;
  LogTransport *transport = log_transport_mock_new(
      TRUE,
      /* utf8 */
      "árvíztűrőtükörfúrógép\n", -1,
      LTM_EOF);

  proto = create_log_proto_text_server_new(transport, 32, LPBS_POS_TRACKING);
  log_proto_set_encoding(proto, "utf-8");
  assert_proto_fetch(proto, "árvíztűrőtükörfúrógép", -1);
  assert_proto_fetch_failure(proto, LPS_EOF, NULL);
  log_proto_free(proto);
}

static void
test_log_proto_text_server_skip_bad_chars(void)
{
  TEST_LOG_PROTO_TEXT_SERVER_SKIP_BAD_CHARS(UTF8_BAD_INPUT, UTF8_BAD_INPUT_SIZE, "utf-8", UTF8_GOOD_OUTPUT);
  TEST_LOG_PROTO_TEXT_SERVER_SKIP_BAD_CHARS(WINDOWS_1250_BAD_INPUT, WINDOWS_1250_BAD_INPUT_SIZE, "windows-1250", WINDOWS_1250_GOOD_OUTPUT);
  TEST_LOG_PROTO_TEXT_SERVER_SKIP_BAD_CHARS(WINDOWS_1252_BAD_INPUT, WINDOWS_1252_BAD_INPUT_SIZE, "windows-1252", WINDOWS_1252_GOOD_OUTPUT);
  TEST_LOG_PROTO_TEXT_SERVER_SKIP_BAD_CHARS(UCS_2_BAD_INPUT, UCS_2_BAD_INPUT_SIZE, "ucs-2le", UCS_2_GOOD_OUTPUT);
  TEST_LOG_PROTO_TEXT_SERVER_SKIP_BAD_CHARS(UTF16_BAD_INPUT, UTF16_BAD_INPUT_SIZE, "utf-16le", UTF16_GOOD_OUTPUT);
}

static void
test_log_proto_text_server_ucs4(void)
{
  LogProto *proto;
  LogTransport *transport = log_transport_mock_new(
      TRUE,
      /* ucs4 */
      "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"      /* |...á...r...v...í| */
      "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\x72"      /* |...z...t...ű...r| */
      "\x00\x00\x01\x51\x00\x00\x00\x74\x00\x00\x00\xfc\x00\x00\x00\x6b"      /* |...Q...t.......k| */
      "\x00\x00\x00\xf6\x00\x00\x00\x72\x00\x00\x00\x66\x00\x00\x00\xfa"      /* |.......r...f....| */
      "\x00\x00\x00\x72\x00\x00\x00\xf3\x00\x00\x00\x67\x00\x00\x00\xe9"      /* |...r.......g....| */
      "\x00\x00\x00\x70\x00\x00\x00\x0a", 88,                                 /* |...p....|         */
      LTM_EOF);

  proto = create_log_proto_text_server_new(transport, 32, 0);
  log_proto_set_encoding(proto, "ucs-4");
  assert_proto_fetch(proto, "árvíztűrőtükörfúrógép", -1);
  assert_proto_fetch_failure(proto, LPS_EOF, NULL);
  log_proto_free(proto);
}

static void
test_log_proto_text_server_iso8859_2(void)
{
  LogProto *proto;
  LogTransport *transport = log_transport_mock_new(
      TRUE,
      /* iso-8859-2 */
      "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa"      /*  |árvíztűrőtükörfú| */
      "\x72\xf3\x67\xe9\x70\n", -1,                                           /*  |rógép|            */
      LTM_EOF);

  proto = create_log_proto_text_server_new(transport, 32, LPBS_POS_TRACKING);
  log_proto_set_encoding(proto, "iso-8859-2");
  assert_proto_fetch(proto, "árvíztűrőtükörfúrógép", -1);
  assert_proto_fetch_failure(proto, LPS_EOF, NULL);
  log_proto_free(proto);
}

static void
test_log_proto_text_server_multi_read(void)
{
  LogProto *proto;
  LogTransport *transport;

  transport = log_transport_mock_new(
      FALSE,
      "foobar\n", -1,
      /* no EOL, proto implementation would read another chunk */
      "foobaz", -1,
      LTM_INJECT_ERROR(EIO),
      LTM_EOF);
  proto = create_log_proto_text_server_new(transport, 32, LPBS_POS_TRACKING);
  assert_proto_fetch(proto, "foobar", -1);
  assert_proto_fetch(proto, "foobaz", -1);
  assert_proto_fetch_failure(proto, LPS_ERROR, NULL);
  log_proto_free(proto);

  transport = log_transport_mock_new(
      FALSE,
      "foobar\n", -1,
      /* no EOL, proto implementation would read another chunk */
      "foobaz", -1,
      LTM_INJECT_ERROR(EIO),
      LTM_EOF);
  proto = create_log_proto_text_server_new(transport, 32, LPBS_POS_TRACKING);
  assert_proto_fetch_single_read(proto, "foobar", -1);
  assert_proto_fetch_single_read(proto, NULL, -1);
  log_proto_free(proto);
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
  test_log_proto_text_server_skip_bad_chars();
}

/****************************************************************************************
 * LogProtoDGramServer
 ****************************************************************************************/

static void
test_log_proto_dgram_server_no_encoding(void)
{
  LogProto *proto;

  proto = log_proto_dgram_server_new(
            log_transport_mock_new(
              FALSE,
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
            32, 0);
  assert_proto_fetch(proto, "0123456789ABCDEF0123456789ABCDEF", -1);
  assert_proto_fetch(proto, "01234567\n", -1);
  assert_proto_fetch(proto, "01234567\0", 9);

  /* no encoding: utf8 remains utf8 */
  assert_proto_fetch(proto, "árvíztűrőtükörfúrógép\n\n", -1);

  /* no encoding: iso-8859-2 remains iso-8859-2 */
  assert_proto_fetch(proto, "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa" /*  |.rv.zt.r.t.k.rf.| */
                            "\x72\xf3\x67\xe9\x70\n",                                          /*  |r.g.p|            */
                            -1);
  /* no encoding, ucs4 becomes a string with embedded NULs */
  assert_proto_fetch(proto, "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"       /* |...á...r...v...í| */
                            "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\x72", 32); /* |...z...t...ű...r|  */

  assert_proto_fetch(proto, "01234", -1);

  log_proto_free(proto);
}


static void
test_log_proto_dgram_server_ucs4(void)
{
  LogProto *proto;

  proto = log_proto_dgram_server_new(
            log_transport_mock_new(
              FALSE,
              /* ucs4, terminated by record size */
              "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"      /* |...á...r...v...í| */
              "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\x72", 32, /* |...z...t...ű...r|  */

              /* ucs4, terminated by ucs4 encododed NL at the end */
              "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"      /* |...á...r...v...í| */
              "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\n", 32,   /* |...z...t...ű|  */

              LTM_EOF),
            32, 0);
  log_proto_set_encoding(proto, "ucs-4");
  assert_proto_fetch(proto, "árvíztűr", -1);
  assert_proto_fetch(proto, "árvíztű\n", -1);
  log_proto_free(proto);
}

static void
test_log_proto_dgram_server_iso_8859_2(void)
{
  LogProto *proto;

  proto = log_proto_dgram_server_new(
            log_transport_mock_new(
              FALSE,

              /* iso-8859-2, deliberately contains
               * accented chars so utf8 representation
               * becomes longer than the record size */
              "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa"       /*  |árvíztűrőtükörfú| */
              "\x72\xf3\x67\xe9\x70\xe9\xe9\xe9\xe9\xe9\xe9\xe9\xe9\xe9\xe9\xe9", -1,  /*  |rógépééééééééééé| */
              LTM_EOF),
            32, 0);
  log_proto_set_encoding(proto, "iso-8859-2");
  assert_proto_fetch(proto, "árvíztűrőtükörfúrógépééééééééééé", -1);
  assert_proto_fetch_ignored_eof(proto);
  log_proto_free(proto);
}

static void
test_log_proto_dgram_server_skip_bad_chars(void)
{
  TEST_LOG_PROTO_DGRAM_SERVER_SKIP_BAD_CHARS(UTF8_BAD_INPUT, UTF8_BAD_INPUT_SIZE, "utf-8", UTF8_GOOD_OUTPUT);
  TEST_LOG_PROTO_DGRAM_SERVER_SKIP_BAD_CHARS(WINDOWS_1250_BAD_INPUT, WINDOWS_1250_BAD_INPUT_SIZE, "windows-1250", WINDOWS_1250_GOOD_OUTPUT);
  TEST_LOG_PROTO_DGRAM_SERVER_SKIP_BAD_CHARS(WINDOWS_1252_BAD_INPUT, WINDOWS_1252_BAD_INPUT_SIZE, "windows-1252", WINDOWS_1252_GOOD_OUTPUT);
  TEST_LOG_PROTO_DGRAM_SERVER_SKIP_BAD_CHARS(UCS_2_BAD_INPUT, UCS_2_BAD_INPUT_SIZE, "ucs-2le", UCS_2_GOOD_OUTPUT);
  TEST_LOG_PROTO_DGRAM_SERVER_SKIP_BAD_CHARS(UTF16_BAD_INPUT, UTF16_BAD_INPUT_SIZE, "utf-16le", UTF16_GOOD_OUTPUT);
}

static void
test_log_proto_dgram_server_eof_handling(void)
{
  LogProto *proto;

  proto = log_proto_dgram_server_new(
            log_transport_mock_new(
              FALSE,
              /* no eol before EOF */
              "01234567", -1,

              LTM_EOF),
            32, 0);
  assert_proto_fetch(proto, "01234567", -1);
  assert_proto_fetch_ignored_eof(proto);
  assert_proto_fetch_ignored_eof(proto);
  assert_proto_fetch_ignored_eof(proto);
  log_proto_free(proto);
}

static void
test_log_proto_dgram_server(void)
{
  test_log_proto_dgram_server_no_encoding();
  test_log_proto_dgram_server_ucs4();
  test_log_proto_dgram_server_iso_8859_2();
  test_log_proto_dgram_server_skip_bad_chars();
  test_log_proto_dgram_server_eof_handling();
}

/****************************************************************************************
 * LogProtoFramedServer
 ****************************************************************************************/

static void
test_log_proto_framed_server_simple_messages(void)
{
  LogProto *proto;

  proto = log_proto_framed_server_new(
            log_transport_mock_new(
              TRUE,
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
            32);
  assert_proto_fetch(proto, "0123456789ABCDEF0123456789ABCDEF", -1);
  assert_proto_fetch(proto, "01234567\n\n", -1);
  assert_proto_fetch(proto, "01234567\0\0", 10);
  assert_proto_fetch(proto, "árvíztűrőtükörfúrógép", -1);
  assert_proto_fetch(proto, "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa"        /*  |.rv.zt.r.t.k.rf.| */
                            "\x72\xf3\x67\xe9\x70", -1);                                              /*  |r.g.p|            */
  assert_proto_fetch(proto, "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"        /* |...á...r...v...í| */
                            "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\x72", 32);  /* |...z...t...q...r|  */
  assert_proto_fetch_failure(proto, LPS_EOF, NULL);
  log_proto_free(proto);
}

static void
test_log_proto_framed_server_io_error(void)
{
  LogProto *proto;

  proto = log_proto_framed_server_new(
            log_transport_mock_new(
              TRUE,
              "32 0123456789ABCDEF0123456789ABCDEF", -1,
              LTM_INJECT_ERROR(EIO),
              LTM_EOF),
            32);
  assert_proto_fetch(proto, "0123456789ABCDEF0123456789ABCDEF", -1);
  assert_proto_fetch_failure(proto, LPS_ERROR, "Error reading frame header");
  log_proto_free(proto);
}


static void
test_log_proto_framed_server_invalid_header(void)
{
  LogProto *proto;

  proto = log_proto_framed_server_new(
            log_transport_mock_new(
              TRUE,
              "1q 0123456789ABCDEF0123456789ABCDEF", -1,
              LTM_EOF),
            32);
  assert_proto_fetch_failure(proto, LPS_ERROR, "Invalid frame header");
  log_proto_free(proto);
}

static void
test_log_proto_framed_server_too_long_line(void)
{
  LogProto *proto;

  proto = log_proto_framed_server_new(
            log_transport_mock_new(
              TRUE,
              "48 0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF", -1,
              LTM_EOF),
            32);
  assert_proto_fetch_failure(proto, LPS_ERROR, "Incoming frame larger than log_msg_size()");
  log_proto_free(proto);
}

static void
test_log_proto_framed_server_message_exceeds_buffer(void)
{
  LogProto *proto;

  /* should cause the buffer to be extended, shifted, as the first message
   * resizes the buffer to 16+10 == 26 bytes. */

  proto = log_proto_framed_server_new(
            log_transport_mock_new(
              FALSE,
              "16 0123456789ABCDE\n16 0123456789ABCDE\n", -1,
              LTM_EOF),
            32);
  assert_proto_fetch(proto, "0123456789ABCDE\n", -1);
  assert_proto_fetch(proto, "0123456789ABCDE\n", -1);
  log_proto_free(proto);
}

static void
test_log_proto_framed_server_buffer_shift_before_fetch(void)
{
  LogProto *proto;

  /* this testcase fills the initially 10 byte buffer with data, which
   * causes a shift in log_proto_framed_server_fetch() */
  proto = log_proto_framed_server_new(
            log_transport_mock_new(
              FALSE,
              "7 012345\n4", 10,
              " 123\n", -1,
              LTM_EOF),
            32);
  assert_proto_fetch(proto, "012345\n", -1);
  assert_proto_fetch(proto, "123\n", -1);
  log_proto_free(proto);
}

static void
test_log_proto_framed_server_buffer_shift_to_make_space_for_a_frame(void)
{
  LogProto *proto;

  /* this testcase fills the initially 10 byte buffer with data, which
   * causes a shift in log_proto_framed_server_fetch() */
  proto = log_proto_framed_server_new(
            log_transport_mock_new(
              FALSE,
              "6 01234\n4 ", 10,
              "123\n", -1,
              LTM_EOF),
            32);
  assert_proto_fetch(proto, "01234\n", -1);
  assert_proto_fetch(proto, "123\n", -1);
  log_proto_free(proto);
}

static void
test_log_proto_framed_server_multi_read(void)
{
  LogProto *proto;

  proto = log_proto_framed_server_new(
            log_transport_mock_new(
              FALSE,
              "7 foobar\n", -1,
              /* no EOL, proto implementation would read another chunk */
              "6 fooba", -1,
              LTM_INJECT_ERROR(EIO),
              LTM_EOF),
            32);
  assert_proto_fetch(proto, "foobar\n", -1);
  /* with multi-read, we get the injected failure at the 2nd fetch */
  assert_proto_fetch_failure(proto, LPS_ERROR, "Error reading frame header");
  log_proto_free(proto);

  /* NOTE: LPBS_NOMREAD is not implemented for framed protocol */
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

  init_and_load_syslogformat_module();

  test_log_proto();

  deinit_syslogformat_module();
  app_shutdown();
  return 0;
}
