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
#include "libtest/mock-transport.h"

#include "transport/transport-haproxy.h"

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

void
_format_addresses(GString *value, LogTransportAuxData *aux)
{
  gchar buf[1024];
  if (aux->peer_addr)
    {
      g_sockaddr_format(aux->peer_addr, buf, sizeof(buf), GSA_FULL);
      g_string_append_printf(value, "source=%s ", buf);
    }
  if (aux->local_addr)
    {
      g_sockaddr_format(aux->local_addr, buf, sizeof(buf), GSA_FULL);
      g_string_append_printf(value, "destination=%s", buf);
    }
}

TestSuite(log_transport_proxy, .init = setup, .fini = teardown);

typedef struct
{
  const gchar *proxy_header;
  gboolean valid;
  const gchar *addresses;
  gint proxy_header_len;
} ProtocolHeaderTestParams;

ParameterizedTestParameters(log_transport_proxy, test_proxy_protocol_parse_header)
{
  static ProtocolHeaderTestParams test_params[] =
  {
    /* SUCCESS */
    { "PROXY UNKNOWN\r\n",                                    TRUE, "" },
    { "PROXY UNKNOWN extra ignored parameters\r\n",           TRUE, "" },
    {
      "PROXY TCP4 1.1.1.1 2.2.2.2 3333 4444\r\n",             TRUE,
      .addresses = "source=AF_INET(1.1.1.1:3333) destination=AF_INET(2.2.2.2:4444)"
    },
    {
      "PROXY TCP6 ::1 ::2 3333 4444\r\n",                     TRUE,
      .addresses = "source=AF_INET6([::1]:3333) destination=AF_INET6([::2]:4444)"
    },

    /* INVALID PROTO */
    { "PROXY UNKNOWN\n",                                      TRUE  }, // WRONG TERMINATION BUT PERMISSIVE IMPLEMENTATION
    { "PROXY TCP4 1.1.1.1 2.2.2.2 3333 4444\n",               TRUE  }, // WRONG TERMINATION BUT PERMISSIVE IMPLEMENTATION
    { "PROXY UNKNOWN\r",                                      FALSE }, // WRONG TERMINATION
    { "PROXY TCP4 1.1.1.1 2.2.2.2 3333 4444\r",               FALSE  }, // WRONG TERMINATION
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

    { "invalid header\r\n",             FALSE},                       // WRONG header

    /* TOO LONG */
    {
      "PROXY TCP4 padpadpadpadpadpadpadpadpadpadpadpadpad"
      "padpadpadpadpadpadpadpadpadpadpadpadpadpadpadpadpad"
      "padpadpadpadpadpadpadpadpadpadpadpadpadpadpadpadpad",  FALSE
    },
    /* proxy v2 */
    {
      "\r\n\r\n\0\r\nQUIT\n!\21\0\f\1\1\1\1\2\2\2\2\2025\255\234", TRUE,
      .addresses = "source=AF_INET(1.1.1.1:33333) destination=AF_INET(2.2.2.2:44444)",
      .proxy_header_len = 28
    },
  };

  return cr_make_param_array(ProtocolHeaderTestParams, test_params, G_N_ELEMENTS(test_params));
}

ParameterizedTest(ProtocolHeaderTestParams *params, log_transport_proxy, test_proxy_protocol_parse_header)
{
  gint proxy_header_len = params->proxy_header_len ? : strlen(params->proxy_header);
  LogTransportStack stack;
  LogTransport *mock = log_transport_mock_stream_new(params->proxy_header, proxy_header_len, NULL);
  LogTransportAuxData aux;
  gchar buf[1024];
  gssize rc;

  log_transport_stack_init(&stack, mock);
  LogTransport *transport = log_transport_haproxy_new(LOG_TRANSPORT_INITIAL, LOG_TRANSPORT_NONE);
  log_transport_assign_to_stack(transport, &stack);

  do
    {
      log_transport_aux_data_init(&aux);
      rc = log_transport_read(transport, buf, sizeof(buf), &aux);
    }
  while (rc == -1 && errno == EAGAIN);

  cr_assert_eq(rc == 0, params->valid,
               "This should be %s: \n>>%.*s<<\n (rc=%d, errno=%d)", params->valid ? "valid" : "invalid", proxy_header_len,
               params->proxy_header, (gint) rc, errno);

  if (rc == 0 && params->addresses)
    {
      GString *addresses = g_string_new(NULL);

      _format_addresses(addresses, &aux);
      cr_assert_str_eq(addresses->str, params->addresses);
      g_string_free(addresses, TRUE);
    }


  log_transport_free(transport);
  log_transport_stack_deinit(&stack);
}
