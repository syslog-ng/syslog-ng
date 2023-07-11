/*
 * Copyright (c) 2020-2023 One Identity
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
#include <criterion/parameterized.h>

#include "transport/transport-proxied-socket-private.h"

#include "apphook.h"
#include "cfg.h"

static void
setup(void)
{
  app_startup();
  configuration = cfg_new_snippet();
}

static void
teardown(void)
{
  if (configuration)
    cfg_free(configuration);
  app_shutdown();
}

static void
fill_proxy_header_buffer(LogTransportProxiedSocket *self, const gchar *msg)
{
  self->proxy_header_buff_len = strlen(msg) + 1;
  g_strlcpy((gchar *)self->proxy_header_buff, msg, sizeof(self->proxy_header_buff));
}

static void
concat_nv(const gchar *name, const gchar *value, gsize value_len, gpointer user_data)
{
  GString *aux_nv_concated = user_data;
  g_string_append_printf(aux_nv_concated, "%s:%s ", name, value);
}

TestSuite(log_transport_proxy, .init = setup, .fini = teardown);

typedef struct
{
  const gchar *proxy_header;
  gboolean valid;
} ProtocolHeaderTestParams;

ParameterizedTestParameters(log_transport_proxy, test_proxy_protocol_parse_header)
{
  static ProtocolHeaderTestParams test_params[] =
  {
    /* SUCCESS */
    { "PROXY UNKNOWN\r\n",                                    TRUE },
    { "PROXY UNKNOWN extra ignored parameters\r\n",           TRUE },
    { "PROXY TCP4 1.1.1.1 2.2.2.2 3333 4444\r\n",             TRUE },
    { "PROXY TCP6 ::1 ::2 3333 4444\r\n",                     TRUE },

    /* INVALID PROTO */
    { "PROXY UNKNOWN\n",                                      TRUE  }, // WRONG TERMINATION BUT PERMISSIVE IMPLEMENTATION
    { "PROXY TCP4 1.1.1.1 2.2.2.2 3333 4444\n",               TRUE  }, // WRONG TERMINATION BUT PERMISSIVE IMPLEMENTATION
    { "PROXY UNKNOWN\r",                                      TRUE  }, // WRONG TERMINATION BUT PERMISSIVE IMPLEMENTATION
    { "PROXY TCP4 1.1.1.1 2.2.2.2 3333 4444\r",               TRUE  }, // WRONG TERMINATION BUT PERMISSIVE IMPLEMENTATION
    { "PROXY\r\n",                                            FALSE },
    { "PROXY TCP4\r\n",                                       FALSE },
    { "PROXY TCP4 1.1.1.1\r\n",                               FALSE },
    { "PROXY TCP4 1.1.1.1 2.2.2.2\r\n",                       FALSE },
    { "PROXY TCP4 1.1.1.1 2.2.2.2 3333\r\n",                  FALSE },
    { "PROXY TCP4 1.1.1.1 2.2.2.2 3333 4444 extra param\r\n", TRUE  },

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

ParameterizedTest(ProtocolHeaderTestParams *params, log_transport_proxy, test_proxy_protocol_parse_header)
{
  LogTransport *transport = log_transport_proxied_stream_socket_new(0);
  LogTransportProxiedSocket *self = (LogTransportProxiedSocket *)transport;

  fill_proxy_header_buffer(self, params->proxy_header);
  cr_assert(_is_proxy_version_v1(self));
  self->proxy_header_version = 1;

  gboolean valid = _parse_proxy_header(self);
  cr_assert_eq(valid, params->valid,
               "This should be %s: %s", params->valid ? "valid" : "invalid", params->proxy_header);

  log_transport_free(transport);
}

Test(log_transport_proxy, test_proxy_invalid_header)
{
  LogTransport *transport = log_transport_proxied_stream_socket_new(0);
  LogTransportProxiedSocket *self = (LogTransportProxiedSocket *)transport;

  fill_proxy_header_buffer(self, "invalid header\r\n");

  cr_assert((! _is_proxy_version_v1(self)) && (! _is_proxy_version_v2(self)));

  log_transport_free(transport);
}

Test(log_transport_proxy, test_proxy_aux_data)
{
  const gchar *expected = "PROXIED_SRCIP:1.1.1.1 PROXIED_DSTIP:2.2.2.2 "
                          "PROXIED_SRCPORT:3333 PROXIED_DSTPORT:4444 "
                          "PROXIED_IP_VERSION:4 ";

  LogTransport *transport = log_transport_proxied_stream_socket_new(0);
  LogTransportProxiedSocket *self = (LogTransportProxiedSocket *)transport;

  fill_proxy_header_buffer(self, "PROXY TCP4 1.1.1.1 2.2.2.2 3333 4444\r\n");
  cr_assert(_is_proxy_version_v1(self));
  self->proxy_header_version = 1;
  cr_assert(_parse_proxy_header(self));

  LogTransportAuxData aux;
  log_transport_aux_data_init(&aux);
  _augment_aux_data(self, &aux);

  GString *aux_nv_concated = g_string_new(NULL);
  log_transport_aux_data_foreach(&aux, concat_nv, aux_nv_concated);
  cr_assert_str_eq(aux_nv_concated->str, expected);

  g_string_free(aux_nv_concated, TRUE);
  log_transport_aux_data_destroy(&aux);
  log_transport_free(transport);
}

Test(log_transport_proxy, test_proxy_v2_header)
{
  const gchar *expected = "PROXIED_SRCIP:1.1.1.1 PROXIED_DSTIP:2.2.2.2 "
                          "PROXIED_SRCPORT:33333 PROXIED_DSTPORT:44444 "
                          "PROXIED_IP_VERSION:4 ";

  LogTransport *transport = log_transport_proxied_stream_socket_new(0);
  LogTransportProxiedSocket *self = (LogTransportProxiedSocket *)transport;

  gchar binary_msg[] = "\r\n\r\n\0\r\nQUIT\n!\21\0\f\1\1\1\1\2\2\2\2\2025\255\234";
  self->proxy_header_buff_len = 28;
  memcpy(self->proxy_header_buff, binary_msg, self->proxy_header_buff_len);

  cr_assert(_is_proxy_version_v2(self));
  self->proxy_header_version = 2;
  cr_assert(_parse_proxy_header(self));

  LogTransportAuxData aux;
  log_transport_aux_data_init(&aux);
  _augment_aux_data(self, &aux);

  GString *aux_nv_concated = g_string_new(NULL);
  log_transport_aux_data_foreach(&aux, concat_nv, aux_nv_concated);
  cr_assert_str_eq(aux_nv_concated->str, expected);

  g_string_free(aux_nv_concated, TRUE);
  log_transport_aux_data_destroy(&aux);
  log_transport_free(transport);
}
