#include "mock-transport.h"
#include "proto_lib.h"
#include "msg_parse_lib.h"
#include "logproto/logproto-dgram-server.h"

/****************************************************************************************
 * LogProtoDGramServer
 ****************************************************************************************/

static void
test_log_proto_dgram_server_no_encoding(void)
{
  LogProtoServer *proto;

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
}


static void
test_log_proto_dgram_server_ucs4(void)
{
  LogProtoServer *proto;

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
}

static void
test_log_proto_dgram_server_invalid_ucs4(void)
{
  LogProtoServer *proto;

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
}

static void
test_log_proto_dgram_server_iso_8859_2(void)
{
  LogProtoServer *proto;

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
}

static void
test_log_proto_dgram_server_eof_handling(void)
{
  LogProtoServer *proto;

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
}

void
test_log_proto_dgram_server(void)
{
  PROTO_TESTCASE(test_log_proto_dgram_server_no_encoding);
  PROTO_TESTCASE(test_log_proto_dgram_server_ucs4);
  PROTO_TESTCASE(test_log_proto_dgram_server_invalid_ucs4);
  PROTO_TESTCASE(test_log_proto_dgram_server_iso_8859_2);
  PROTO_TESTCASE(test_log_proto_dgram_server_eof_handling);
}
