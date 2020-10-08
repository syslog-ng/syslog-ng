/*
 * Copyright (c) 2020 One Identity
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
#include "logproto/logproto-proxied-text-server.h"
#include "logproto/logproto-proxied-text-server-internal.h"

#include "mock-transport.h"
#include "proto_lib.h"

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

typedef struct
{
  const guchar *proxy_header;
  gboolean valid;
} ProtocolHeaderTestParams;

ParameterizedTestParameters(log_proto, test_proxy_protocol_parse_header)
{
  static ProtocolHeaderTestParams test_params[] =
  {
    /* SUCCESS */
    { "PROXY UNKNOWN\r\n",                                    TRUE },
    { "PROXY UNKNOWN extra ignored parameters\r\n",           TRUE },
    { "PROXY TCP4 1.1.1.1 2.2.2.2 3333 4444\r\n",             TRUE },
    { "PROXY TCP6 ::1 ::2 3333 4444\r\n",                     TRUE },

    /* INVALID PROTO */
    { "PROXY UNKNOWN\n",                                      FALSE }, // WRONG TERMINATION
    { "PROXY TCP4 1.1.1.1 2.2.2.2 3333 4444\n",               FALSE }, // WRONG TERMINATION
    { "PROXY UNKNOWN\r",                                      FALSE }, // WRONG TERMINATION
    { "PROXY TCP4 1.1.1.1 2.2.2.2 3333 4444\r",               FALSE }, // WRONG TERMINATION
    { "PROXY UNKNOWN",                                        FALSE }, // WRONG TERMINATION
    { "PROXY TCP4 1.1.1.1 2.2.2.2 3333 4444",                 FALSE }, // WRONG TERMINATION
    { "PROXY\r\n",                                            FALSE },
    { "PROXY TCP4\r\n",                                       FALSE },
    { "PROXY TCP4 1.1.1.1\r\n",                               FALSE },
    { "PROXY TCP4 1.1.1.1 2.2.2.2\r\n",                       FALSE },
    { "PROXY TCP4 1.1.1.1 2.2.2.2 3333\r\n",                  FALSE },
    { "PROXY TCP4 1.1.1.1 2.2.2.2 3333 4444 extra param\r\n", FALSE },

    /* EXTRA WHITESPACE */
    { "PROXY  TCP4 1.1.1.1 2.2.2.2 3333 4444\r\n",            FALSE },
    { "PROXY TCP4  1.1.1.1 2.2.2.2 3333 4444\r\n",            FALSE },
    { "PROXY TCP4 1.1.1.1  2.2.2.2 3333 4444\r\n",            FALSE },
    { "PROXY TCP4 1.1.1.1 2.2.2.2  3333 4444\r\n",            FALSE },
    { "PROXY TCP4 1.1.1.1 2.2.2.2 3333  4444\r\n",            FALSE },
    { "PROXY TCP4 1.1.1.1 2.2.2.2 3333 4444 \r\n",            FALSE },

    /* INVALID ARGUMENTS */
    { "PROXY TCP3 1.1.1.1 2.2.2.2 3333 4444\r\n",             FALSE }, // WRONG IP PROTO
    { "PROXY TCP6 1.1.1.1 2.2.2.2 3333 4444\r\n",             FALSE }, // WRONG IP PROTO
    { "PROXY TCP4 ::1 ::2 3333 4444\r\n",                     FALSE }, // WRONG IP PROTO
    { "PROXY TCP4 1.1.1 2.2.2.2 3333 4444\r\n",               FALSE }, // WRONG IP
    { "PROXY TCP4 1.1.1.1.1 2.2.2.2 3333 4444\r\n",           FALSE }, // WRONG IP
    { "PROXY TCP6 ::1::0 ::1 3333 4444\r\n",                  FALSE }, // WRONG IP
    { "PROXY TCP4 1.1.1.1 2.2.2.2 33333 0\r\n",               FALSE }, // WRONG PORT
    { "PROXY TCP4 1.1.1.1 2.2.2.2 33333 -1\r\n",              FALSE }, // WRONG PORT
    { "PROXY TCP4 1.1.1.1 2.2.2.2 33333 65536\r\n",           FALSE }, // WRONG PORT

    { "PROXY TCP4 padpadpadpadpadpadpadpadpadpadpadpadpad"
      "padpadpadpadpadpadpadpadpadpadpadpadpadpadpadpadpad"
      "padpadpadpadpadpadpadpadpadpadpadpadpadpadpadpadpad",  FALSE } // TOO LONG
  };

  return cr_make_param_array(ProtocolHeaderTestParams, test_params, G_N_ELEMENTS(test_params));
}

ParameterizedTest(ProtocolHeaderTestParams *params, log_proto, test_proxy_protocol_parse_header)
{
  LogProtoServer *proto = log_proto_proxied_text_server_new(log_transport_mock_records_new("", -1, LTM_EOF),
                                                            get_inited_proto_server_options());
  gboolean valid = _log_proto_proxied_text_server_parse_header((LogProtoProxiedTextServer *)proto,
                                                               params->proxy_header,
                                                               strlen(params->proxy_header));

  cr_assert_eq(valid, params->valid,
               "This should be %s: %s", params->valid ? "valid" : "invalid", params->proxy_header);

  log_proto_server_free(proto);
}

Test(log_proto, test_proxy_protocol_handshake_and_fetch_success)
{
  LogTransport *transport = log_transport_mock_records_new("PROXY TCP4 1.1.1.1 2.2.2.2 3333 4444\r\n", -1,
                                                           "test message\n", -1,
                                                           LTM_EOF);
  LogProtoServer *proto = log_proto_proxied_text_server_new(transport, get_inited_proto_server_options());

  cr_assert_eq(log_proto_server_handshake(proto), LPS_SUCCESS);
  assert_proto_server_fetch(proto, "test message", -1);

  log_proto_server_free(proto);
}

Test(log_proto, test_proxy_protocol_handshake_failed)
{
  LogTransport *transport = log_transport_mock_records_new("invalid header\r\n", -1,
                                                           "test message\n", -1,
                                                           LTM_EOF);
  LogProtoServer *proto = log_proto_proxied_text_server_new(transport, get_inited_proto_server_options());

  cr_assert_eq(log_proto_server_handshake(proto), LPS_ERROR);

  log_proto_server_free(proto);
}
