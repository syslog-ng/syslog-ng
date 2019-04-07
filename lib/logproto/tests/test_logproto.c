/*
 * Copyright (c) 2012-2019 Balabit
 * Copyright (c) 2012-2013 Balázs Scheidler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "mock-transport.h"
#include "proto_lib.h"
#include "msg_parse_lib.h"
#include "logproto/logproto-text-server.h"
#include "logproto/logproto-framed-server.h"
#include "logproto/logproto-dgram-server.h"
#include "logproto/logproto-record-server.h"
#include "apphook.h"

#include <criterion/criterion.h>

void
setup(void)
{
  app_startup();
  init_proto_tests();
}

void
teardown(void)
{
  deinit_proto_tests();
  app_shutdown();
}

Test(log_proto, test_base)
{
  cr_assert_eq(log_proto_get_char_size_for_fixed_encoding("iso-8859-2"), 1);
  cr_assert_eq(log_proto_get_char_size_for_fixed_encoding("ucs-4"), 4);

  log_proto_server_options_set_encoding(&proto_server_options, "ucs-4");
  LogProtoServer *proto = log_proto_binary_record_server_new(
                            log_transport_mock_records_new(
                              /* ucs4, terminated by record size */
                              "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"      /* |...á...r...v...í| */
                              "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\x72", 32, /* |...z...t...ű...r|  */
                              LTM_EOF),
                            get_inited_proto_server_options(), 32);

  /* check if error state is not forgotten unless reset_error is called */
  proto->status = LPS_ERROR;
  assert_proto_server_status(proto, proto->status, LPS_ERROR);
  assert_proto_server_fetch_failure(proto, LPS_ERROR, NULL);

  log_proto_server_reset_error(proto);
  assert_proto_server_fetch(proto, "árvíztűr", -1);
  assert_proto_server_status(proto, proto->status, LPS_SUCCESS);

  log_proto_server_free(proto);
}

Test(log_proto, test_log_proto, .disabled = true)
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
}

TestSuite(log_proto, .init = setup, .fini = teardown);
