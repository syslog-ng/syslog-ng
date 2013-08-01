#include "test_logproto.h"
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
  test_log_proto_indented_multiline_server();
  test_log_proto_regexp_multiline_server();
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
