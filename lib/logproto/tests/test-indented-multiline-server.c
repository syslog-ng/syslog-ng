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
#include "logproto/logproto-indented-multiline-server.h"

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

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

Test(log_proto, test_proper_multiline)
{
  test_proper_multiline(TRUE);
  test_proper_multiline(FALSE);
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

Test(log_proto, test_line_without_continuation)
{
  test_line_without_continuation(TRUE);
  test_line_without_continuation(FALSE);
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

Test(log_proto, test_input_starts_with_continuation)
{
  test_input_starts_with_continuation(TRUE);
  test_input_starts_with_continuation(FALSE);
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

Test(log_proto, test_multiline_at_eof)
{
  test_multiline_at_eof(TRUE);
  test_multiline_at_eof(FALSE);
}
