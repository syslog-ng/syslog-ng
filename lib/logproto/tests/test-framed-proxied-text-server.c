/*
 * Copyright (c) 2023 One Identity LLC.
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

#include <criterion/criterion.h>
#include "libtest/mock-transport.h"
#include "libtest/proto_lib.h"
#include "libtest/msg_parse_lib.h"

#include "logproto/logproto-framed-proxied-text-server.h"

#include <errno.h>

Test(log_proto, test_proxy_protocol_handshake_and_fetch_success_001)
{
  LogTransport *transport = log_transport_mock_records_new("PROXY TCP4 1.1.1.1 2.2.2.2 3333 4444\r\n", -1,
                                                           "25 This is the first message26 This is the second message\n", -1,
                                                           LTM_EOF);
  LogProtoServer *proto = log_proto_framed_proxied_text_server_new(transport, get_inited_proto_server_options());

  cr_assert_eq(log_proto_server_handshake(proto), LPS_SUCCESS);
  assert_proto_server_fetch(proto, "This is the first message", -1);
  assert_proto_server_fetch(proto, "This is the second message", -1);

  log_proto_server_free(proto);
}
