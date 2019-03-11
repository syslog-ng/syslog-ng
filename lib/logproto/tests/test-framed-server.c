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
#include "logproto/logproto-framed-server.h"

#include <errno.h>
#include <criterion/criterion.h>

Test(log_proto, test_log_proto_framed_server_simple_messages)
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
  assert_proto_server_fetch(proto,
                            "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa"        /*  |.rv.zt.r.t.k.rf.| */
                            "\x72\xf3\x67\xe9\x70", -1);                                              /*  |r.g.p|            */
  assert_proto_server_fetch(proto,
                            "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"        /* |...á...r...v...í| */
                            "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\x72", 32);  /* |...z...t...q...r|  */
  assert_proto_server_fetch_failure(proto, LPS_EOF, NULL);
  log_proto_server_free(proto);
}

Test(log_proto, test_log_proto_framed_server_io_error)
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
  assert_proto_server_fetch_failure(proto, LPS_ERROR, "Error reading RFC6587 style framed data");
  log_proto_server_free(proto);
}


Test(log_proto, test_log_proto_framed_server_invalid_header)
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

Test(log_proto, test_log_proto_framed_server_too_long_line)
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

Test(log_proto, test_log_proto_framed_server_message_exceeds_buffer)
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

Test(log_proto, test_log_proto_framed_server_buffer_shift_before_fetch)
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

Test(log_proto, test_log_proto_framed_server_buffer_shift_to_make_space_for_a_frame)
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

Test(log_proto, test_log_proto_framed_server_multi_read)
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
  assert_proto_server_fetch_failure(proto, LPS_ERROR, "Error reading RFC6587 style framed data");
  log_proto_server_free(proto);

  /* NOTE: LPBS_NOMREAD is not implemented for framed protocol */
}
