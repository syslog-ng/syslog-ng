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

#include "mock-transport.h"
#include "proto_lib.h"

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

#include <errno.h>

typedef struct
{
  const gchar *proxy_header;
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
    { "PROXY UNKNOWN\n",                                      TRUE }, // WRONG TERMINATION
    { "PROXY TCP4 1.1.1.1 2.2.2.2 3333 4444\n",               TRUE }, // WRONG TERMINATION
    { "PROXY UNKNOWN\r",                                      TRUE }, // WRONG TERMINATION
    { "PROXY TCP4 1.1.1.1 2.2.2.2 3333 4444\r",               TRUE }, // WRONG TERMINATION
    { "PROXY\r\n",                                            FALSE },
    { "PROXY TCP4\r\n",                                       FALSE },
    { "PROXY TCP4 1.1.1.1\r\n",                               FALSE },
    { "PROXY TCP4 1.1.1.1 2.2.2.2\r\n",                       FALSE },
    { "PROXY TCP4 1.1.1.1 2.2.2.2 3333\r\n",                  FALSE },
    { "PROXY TCP4 1.1.1.1 2.2.2.2 3333 4444 extra param\r\n", TRUE},

    /* EXTRA WHITESPACE - PERMISSIVE */
    { "PROXY TCP4  1.1.1.1 2.2.2.2 3333 4444\r\n",            TRUE },
    { "PROXY TCP4 1.1.1.1  2.2.2.2 3333 4444\r\n",            TRUE },
    { "PROXY TCP4 1.1.1.1 2.2.2.2  3333 4444\r\n",            TRUE },
    { "PROXY TCP4 1.1.1.1 2.2.2.2 3333  4444\r\n",            TRUE },
    { "PROXY TCP4 1.1.1.1 2.2.2.2 3333 4444 \r\n",            TRUE },

    /* EXTRA WHITESPACE BEFORE PARAMETERS */
    { "PROXY  TCP4 1.1.1.1 2.2.2.2 3333 4444\r\n",            FALSE },


    /* INVALID ARGUMENTS - PERMISSIVE */
    { "PROXY TCP6 1.1.1.1 2.2.2.2 3333 4444\r\n",             TRUE }, // WRONG IP PROTO
    { "PROXY TCP4 ::1 ::2 3333 4444\r\n",                     TRUE }, // WRONG IP PROTO
    { "PROXY TCP4 1.1.1 2.2.2.2 3333 4444\r\n",               TRUE }, // WRONG IP
    { "PROXY TCP4 1.1.1.1.1 2.2.2.2 3333 4444\r\n",           TRUE }, // WRONG IP
    { "PROXY TCP6 ::1::0 ::1 3333 4444\r\n",                  TRUE }, // WRONG IP
    { "PROXY TCP4 1.1.1.1 2.2.2.2 33333 0\r\n",               TRUE }, // WRONG PORT
    { "PROXY TCP4 1.1.1.1 2.2.2.2 33333 -1\r\n",              TRUE }, // WRONG PORT
    { "PROXY TCP4 1.1.1.1 2.2.2.2 33333 65536\r\n",           TRUE }, // WRONG PORT

    /* INVALID ARGUMENT(S)*/
    { "PROXY TCP3 1.1.1.1 2.2.2.2 3333 4444\r\n",             FALSE}, // WRONG IP PROTO


    {
      "PROXY TCP4 padpadpadpadpadpadpadpadpadpadpadpadpad"
      "padpadpadpadpadpadpadpadpadpadpadpadpadpadpadpadpad"
      "padpadpadpadpadpadpadpadpadpadpadpadpadpadpadpadpad",  FALSE
    } // TOO LONG
  };

  return cr_make_param_array(ProtocolHeaderTestParams, test_params, G_N_ELEMENTS(test_params));
}

ParameterizedTest(ProtocolHeaderTestParams *params, log_proto, test_proxy_protocol_parse_header)
{
  LogProtoServer *proto = log_proto_proxied_text_server_new(log_transport_mock_records_new(params->proxy_header,
                                                            -1, LTM_EOF),
                                                            get_inited_proto_server_options());

  gboolean valid = log_proto_server_handshake(proto) == LPS_SUCCESS;
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

static void
get_aux_data_from_next_message(LogProtoServer *proto, LogTransportAuxData *aux)
{
  LogProtoStatus status;

  const guchar *msg = NULL;
  gsize msg_len = 0;
  Bookmark bookmark;
  gboolean may_read = TRUE;

  do
    {
      log_transport_aux_data_init(aux);
      status = log_proto_server_fetch(proto, &msg, &msg_len, &may_read, aux, &bookmark);
      if (status == LPS_AGAIN)
        status = LPS_SUCCESS;
    }
  while (status == LPS_SUCCESS && msg == NULL && may_read);
}

static void
concat_nv(const gchar *name, const gchar *value, gsize value_len, gpointer user_data)
{
  GString *aux_nv_concated = user_data;
  g_string_append_printf(aux_nv_concated, "%s:%s ", name, value);
}

Test(log_proto, test_proxy_protocol_aux_data)
{
  const gchar *expected = "PROXIED_SRCIP:1.1.1.1 PROXIED_DSTIP:2.2.2.2 "
                          "PROXIED_SRCPORT:3333 PROXIED_DSTPORT:4444 "
                          "PROXIED_IP_VERSION:4 ";
  LogTransport *transport = log_transport_mock_records_new("PROXY TCP4 1.1.1.1 2.2.2.2 3333 4444\r\n", -1,
                                                           "test message\n", -1,
                                                           LTM_EOF);
  LogProtoServer *proto = log_proto_proxied_text_server_new(transport, get_inited_proto_server_options());

  cr_assert_eq(log_proto_server_handshake(proto), LPS_SUCCESS);

  LogTransportAuxData aux;
  log_transport_aux_data_init(&aux);
  get_aux_data_from_next_message(proto, &aux);

  GString *aux_nv_concated = g_string_new(NULL);
  log_transport_aux_data_foreach(&aux, concat_nv, aux_nv_concated);
  cr_assert_str_eq(aux_nv_concated->str, expected);

  g_string_free(aux_nv_concated, TRUE);
  log_transport_aux_data_destroy(&aux);
  log_proto_server_free(proto);
}


static void
assert_handshake_is_taking_place(LogProtoServer *proto)
{
  gint timeout;
  GIOCondition cond;


  cr_assert_eq(log_proto_server_prepare(proto, &cond, &timeout), LPPA_POLL_IO);
  cr_assert_eq(cond, G_IO_IN);

  cr_assert(log_proto_server_handshake_in_progress(proto));

}

Test(log_proto, test_proxy_protocol_header_partial_read)
{
  LogTransportMock *transport = (LogTransportMock *) log_transport_mock_records_new(LTM_EOF);
  const char *proxy_header_segments[] = {"P", "ROXY TCP4 ", "1.1.1.1 ", "2.2.2.2 3333 ", "4444\r\n"};
  size_t length = G_N_ELEMENTS(proxy_header_segments);

  for(size_t i = 0; i < length; i++)
    {
      log_transport_mock_inject_data(transport, proxy_header_segments[i], -1);
      log_transport_mock_inject_data(transport, LTM_INJECT_ERROR(EAGAIN));
    }

  log_transport_mock_inject_data(transport, "test message\n", -1);

  LogProtoServer *proto = log_proto_proxied_text_server_new((LogTransport *) transport,
                                                            get_inited_proto_server_options());

  for(size_t i = 0; i < length - 1; i++)
    {
      assert_handshake_is_taking_place(proto);
      cr_assert_eq(log_proto_server_handshake(proto), LPS_AGAIN);
    }

  cr_assert_eq(log_proto_server_handshake(proto), LPS_SUCCESS);
  cr_assert_not(log_proto_server_handshake_in_progress(proto));



  const gchar *expected = "PROXIED_SRCIP:1.1.1.1 PROXIED_DSTIP:2.2.2.2 "
                          "PROXIED_SRCPORT:3333 PROXIED_DSTPORT:4444 "
                          "PROXIED_IP_VERSION:4 ";
  LogTransportAuxData aux;
  log_transport_aux_data_init(&aux);
  get_aux_data_from_next_message(proto, &aux);

  GString *aux_nv_concated = g_string_new(NULL);
  log_transport_aux_data_foreach(&aux, concat_nv, aux_nv_concated);
  cr_assert_str_eq(aux_nv_concated->str, expected);

  g_string_free(aux_nv_concated, TRUE);
  log_transport_aux_data_destroy(&aux);




  log_proto_server_free(proto);

}
