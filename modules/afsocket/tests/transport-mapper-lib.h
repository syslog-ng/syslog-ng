/*
 * Copyright (c) 2013 Balabit
 * Copyright (c) 2013 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#ifndef AFSOCKET_OPTIONS_LIB_H_INCLUDED
#define AFSOCKET_OPTIONS_LIB_H_INCLUDED

#include "transport-mapper.h"
#include "testutils.h"

extern TransportMapper *transport_mapper;

#define transport_mapper_testcase_begin(init_name, func, args)          \
  do                                                            \
    {                                                           \
      testcase_begin("%s(%s)", func, args);                     \
      transport_mapper = transport_mapper_ ## init_name ## _new();          \
    }                                                           \
  while (0)

#define transport_mapper_testcase_end()                           \
  do                                                            \
    {                                                           \
      transport_mapper_free(transport_mapper);              \
      testcase_end();                                           \
    }                                                           \
  while (0)

#define TRANSPORT_MAPPER_TESTCASE(init_name, x, ...) \
  do {                                                                    \
      transport_mapper_testcase_begin(init_name, #x, #__VA_ARGS__);  \
      x(__VA_ARGS__);                                                     \
      transport_mapper_testcase_end();                               \
  } while(0)

static inline void
assert_transport_mapper_apply(TransportMapper *self, const gchar *transport)
{
  if (transport)
    transport_mapper_set_transport(self, transport);
  assert_true(transport_mapper_apply_transport(self, configuration), "afsocket_apply_transport() failed");
}

static inline void
assert_transport_mapper_apply_fails(TransportMapper *self, const gchar *transport)
{
  if (transport)
    transport_mapper_set_transport(self, transport);
  assert_false(transport_mapper_apply_transport(self, configuration),
               "afsocket_apply_transport() succeeded while we expected failure");
}

static inline void
assert_transport_mapper_transport(TransportMapper *options, const gchar *expected_transport)
{
  assert_string(options->transport, expected_transport, "TransportMapper contains a mismatching transport name");
}

static inline void
assert_transport_mapper_logproto(TransportMapper *options, const gchar *expected_logproto)
{
  assert_string(options->logproto, expected_logproto, "TransportMapper contains a mismatching log_proto name");
}

static inline void
assert_transport_mapper_stats_source(TransportMapper *options, gint stats_source)
{
  assert_gint(options->stats_source, stats_source, "TransportMapper contains a mismatching stats_source");
}

static inline void
assert_transport_mapper_address_family(TransportMapper *options, gint address_family)
{
  assert_gint(options->address_family, address_family, "TransportMapper address family mismatch");
}

static inline void
assert_transport_mapper_sock_type(TransportMapper *options, gint sock_type)
{
  assert_gint(options->sock_type, sock_type, "TransportMapper sock_type mismatch");
}

static inline void
assert_transport_mapper_sock_proto(TransportMapper *options, gint sock_proto)
{
  assert_gint(options->sock_proto, sock_proto, "TransportMapper sock_proto mismatch");
}

#endif
