/*
 * Copyright (c) 2013 Balabit
 * Copyright (c) 2013 Balázs Scheidler <balazs.scheidler@balabit.com>
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

#include "afinet.h"
#include "afsocket.h"
#include "stats/stats-registry.h"
#include "apphook.h"
#include "transport-mapper-inet.h"
#include "socket-options-inet.h"
#include "transport-mapper-lib.h"

#include <unistd.h>

static inline void
assert_transport_mapper_inet_server_port(TransportMapper *s, gint server_port)
{
  cr_assert_eq(transport_mapper_inet_get_server_port(s), server_port, "TransportMapper server_port mismatch");
}

static void
assert_transport_mapper_tcp_socket(TransportMapper *self)
{
  assert_transport_mapper_address_family(self, AF_INET);
  assert_transport_mapper_sock_type(self, SOCK_STREAM);
  assert_transport_mapper_sock_proto(self, IPPROTO_TCP);
}

static void
assert_transport_mapper_tcp6_socket(TransportMapper *self)
{
  assert_transport_mapper_address_family(self, AF_INET6);
  assert_transport_mapper_sock_type(self, SOCK_STREAM);
  assert_transport_mapper_sock_proto(self, IPPROTO_TCP);
}

static void
assert_transport_mapper_udp_socket(TransportMapper *self)
{
  assert_transport_mapper_address_family(self, AF_INET);
  assert_transport_mapper_sock_type(self, SOCK_DGRAM);
  assert_transport_mapper_sock_proto(self, IPPROTO_UDP);
}

static void
assert_transport_mapper_udp6_socket(TransportMapper *self)
{
  assert_transport_mapper_address_family(self, AF_INET6);
  assert_transport_mapper_sock_type(self, SOCK_DGRAM);
  assert_transport_mapper_sock_proto(self, IPPROTO_UDP);
}

static gboolean
create_socket_with_address(GSockAddr *addr, gint *sock)
{
  SocketOptionsInet *sock_options;
  gboolean result;

  sock_options = socket_options_inet_new_instance();
  result = transport_mapper_open_socket(transport_mapper, &sock_options->super, addr, AFSOCKET_DIR_RECV, sock);
  socket_options_free(&sock_options->super);
  return result;
}

static gboolean
create_socket(gint *sock)
{
  GSockAddr *addr = g_sockaddr_inet_new("127.0.0.1", 0);
  gboolean success;

  success = create_socket_with_address(addr, sock);
  g_sockaddr_unref(addr);
  return success;
}

static void
assert_create_socket_fails_with_address(GSockAddr *addr)
{
  gint sock;

  cr_assert_not(create_socket_with_address(addr, &sock), "transport_mapper_open_socket() succeeded unexpectedly");
  cr_assert_eq(sock, -1, "failed create_socket returned a non-extremal value on failure");
}

static void
assert_create_socket_fails(void)
{
  gint sock;

  cr_assert_not(create_socket(&sock), "transport_mapper_open_socket() succeeded unexpectedly");
  cr_assert_eq(sock, -1, "failed create_socket returned a non-extremal value on failure");
}

static void
assert_create_socket_succeeds(void)
{
  gint sock;

  cr_assert(create_socket(&sock), "transport_mapper_open_socket() failed unexpectedly");
  close(sock);
}

static TLSContext *
create_dummy_tls_context(void)
{
  TLSContext *tls_context;

  tls_context = tls_context_new(TM_SERVER, "dummy-location");
  return tls_context;
}

/******************************************************************************
 * Tests start here
 ******************************************************************************/

Test(transport_mapper_inet, test_tcp_apply_transport_sets_defaults)
{
  transport_mapper = transport_mapper_tcp_new();
  assert_transport_mapper_apply(transport_mapper, NULL);
  assert_transport_mapper_tcp_socket(transport_mapper);

  assert_transport_mapper_transport(transport_mapper, "tcp");
  assert_transport_mapper_logproto(transport_mapper, "text");
  assert_transport_mapper_stats_source(transport_mapper, SCS_TCP);
  assert_transport_mapper_inet_server_port(transport_mapper, 514);
}

Test(transport_mapper_inet, test_tcp6_apply_transport_sets_defaults)
{
  transport_mapper = transport_mapper_tcp6_new();
  assert_transport_mapper_apply(transport_mapper, NULL);
  assert_transport_mapper_tcp6_socket(transport_mapper);

  assert_transport_mapper_transport(transport_mapper, "tcp");
  assert_transport_mapper_logproto(transport_mapper, "text");
  assert_transport_mapper_stats_source(transport_mapper, SCS_TCP6);
  assert_transport_mapper_inet_server_port(transport_mapper, 514);
}

Test(transport_mapper_inet, test_udp_apply_transport_sets_defaults)
{
  transport_mapper = transport_mapper_udp_new();
  assert_transport_mapper_apply(transport_mapper, NULL);
  assert_transport_mapper_udp_socket(transport_mapper);

  assert_transport_mapper_transport(transport_mapper, "udp");
  assert_transport_mapper_logproto(transport_mapper, "dgram");
  assert_transport_mapper_stats_source(transport_mapper, SCS_UDP);
  assert_transport_mapper_inet_server_port(transport_mapper, 514);
}

Test(transport_mapper_inet, test_udp_apply_fails_when_tls_context_is_set)
{
  transport_mapper = transport_mapper_udp_new();
  transport_mapper_inet_set_tls_context((TransportMapperInet *) transport_mapper, create_dummy_tls_context(), NULL);
  assert_transport_mapper_apply_fails(transport_mapper, "udp");
}

Test(transport_mapper_inet, test_udp6_apply_transport_sets_defaults)
{
  transport_mapper = transport_mapper_udp6_new();
  assert_transport_mapper_apply(transport_mapper, NULL);
  assert_transport_mapper_udp6_socket(transport_mapper);

  assert_transport_mapper_transport(transport_mapper, "udp");
  assert_transport_mapper_logproto(transport_mapper, "dgram");
  assert_transport_mapper_stats_source(transport_mapper, SCS_UDP6);
  assert_transport_mapper_inet_server_port(transport_mapper, 514);
}

Test(transport_mapper_inet, test_network_transport_udp_apply_transport_sets_defaults)
{
  transport_mapper = transport_mapper_network_new();
  assert_transport_mapper_apply(transport_mapper, "udp");
  assert_transport_mapper_udp_socket(transport_mapper);

  assert_transport_mapper_transport(transport_mapper, "udp");
  assert_transport_mapper_logproto(transport_mapper, "dgram");
  assert_transport_mapper_stats_source(transport_mapper, SCS_NETWORK);
  assert_transport_mapper_inet_server_port(transport_mapper, 514);
}

Test(transport_mapper_inet, test_network_transport_udp_apply_fails_when_tls_context_is_set)
{
  transport_mapper = transport_mapper_network_new();
  transport_mapper_inet_set_tls_context((TransportMapperInet *) transport_mapper, create_dummy_tls_context(), NULL);
  assert_transport_mapper_apply_fails(transport_mapper, "udp");
}

Test(transport_mapper_inet, test_network_transport_tcp_apply_transport_sets_defaults)
{
  transport_mapper = transport_mapper_network_new();
  assert_transport_mapper_apply(transport_mapper, "tcp");
  assert_transport_mapper_tcp_socket(transport_mapper);

  assert_transport_mapper_transport(transport_mapper, "tcp");
  assert_transport_mapper_logproto(transport_mapper, "text");
  assert_transport_mapper_stats_source(transport_mapper, SCS_NETWORK);
  assert_transport_mapper_inet_server_port(transport_mapper, 514);
}

Test(transport_mapper_inet, test_network_transport_tls_apply_fails_without_tls_context)
{
  transport_mapper = transport_mapper_network_new();
  assert_transport_mapper_apply_fails(transport_mapper, "tls");
}

Test(transport_mapper_inet, test_network_transport_tls_apply_transport_sets_defaults)
{
  transport_mapper = transport_mapper_network_new();
  transport_mapper_inet_set_tls_context((TransportMapperInet *) transport_mapper, create_dummy_tls_context(), NULL);
  assert_transport_mapper_apply(transport_mapper, "tls");
  assert_transport_mapper_tcp_socket(transport_mapper);

  assert_transport_mapper_transport(transport_mapper, "tls");
  assert_transport_mapper_logproto(transport_mapper, "text");
  assert_transport_mapper_stats_source(transport_mapper, SCS_NETWORK);
  assert_transport_mapper_inet_server_port(transport_mapper, 514);
}

Test(transport_mapper_inet, test_network_transport_foo_apply_transport_sets_defaults)
{
  transport_mapper = transport_mapper_network_new();
  assert_transport_mapper_apply(transport_mapper, "foo");
  assert_transport_mapper_tcp_socket(transport_mapper);

  assert_transport_mapper_transport(transport_mapper, "foo");
  assert_transport_mapper_logproto(transport_mapper, "foo");
  assert_transport_mapper_stats_source(transport_mapper, SCS_NETWORK);
  assert_transport_mapper_inet_server_port(transport_mapper, 514);
}

Test(transport_mapper_inet, test_syslog_transport_udp_apply_transport_sets_defaults)
{
  transport_mapper = transport_mapper_syslog_new();
  assert_transport_mapper_apply(transport_mapper, "udp");
  assert_transport_mapper_udp_socket(transport_mapper);

  assert_transport_mapper_transport(transport_mapper, "udp");
  assert_transport_mapper_logproto(transport_mapper, "dgram");
  assert_transport_mapper_stats_source(transport_mapper, SCS_SYSLOG);
  assert_transport_mapper_inet_server_port(transport_mapper, 514);
}

Test(transport_mapper_inet, test_syslog_transport_udp_apply_fails_when_tls_context_is_set)
{
  transport_mapper = transport_mapper_syslog_new();
  transport_mapper_inet_set_tls_context((TransportMapperInet *) transport_mapper, create_dummy_tls_context(), NULL);
  assert_transport_mapper_apply_fails(transport_mapper, "udp");
}

Test(transport_mapper_inet, test_syslog_transport_tcp_apply_transport_sets_defaults)
{
  transport_mapper = transport_mapper_syslog_new();
  assert_transport_mapper_apply(transport_mapper, "tcp");
  assert_transport_mapper_tcp_socket(transport_mapper);

  assert_transport_mapper_transport(transport_mapper, "tcp");
  assert_transport_mapper_logproto(transport_mapper, "framed");
  assert_transport_mapper_stats_source(transport_mapper, SCS_SYSLOG);
  assert_transport_mapper_inet_server_port(transport_mapper, 601);
}

Test(transport_mapper_inet, test_syslog_transport_tls_apply_fails_without_tls_context)
{
  transport_mapper = transport_mapper_syslog_new();
  assert_transport_mapper_apply_fails(transport_mapper, "tls");
}

Test(transport_mapper_inet, test_syslog_transport_tls_apply_transport_sets_defaults)
{
  transport_mapper = transport_mapper_syslog_new();
  transport_mapper_inet_set_tls_context((TransportMapperInet *) transport_mapper, create_dummy_tls_context(), NULL);
  assert_transport_mapper_apply(transport_mapper, "tls");
  assert_transport_mapper_tcp_socket(transport_mapper);

  assert_transport_mapper_transport(transport_mapper, "tls");
  assert_transport_mapper_logproto(transport_mapper, "framed");
  assert_transport_mapper_stats_source(transport_mapper, SCS_SYSLOG);
  assert_transport_mapper_inet_server_port(transport_mapper, 6514);
}

Test(transport_mapper_inet, test_syslog_transport_foo_apply_transport_sets_defaults)
{
  transport_mapper = transport_mapper_syslog_new();
  assert_transport_mapper_apply(transport_mapper, "foo");
  assert_transport_mapper_tcp_socket(transport_mapper);

  assert_transport_mapper_transport(transport_mapper, "foo");
  assert_transport_mapper_logproto(transport_mapper, "foo");
  assert_transport_mapper_stats_source(transport_mapper, SCS_SYSLOG);
  assert_transport_mapper_inet_server_port(transport_mapper, 514);
}

Test(transport_mapper_inet, test_open_socket_opens_a_socket_and_applies_socket_options)
{
  transport_mapper = transport_mapper_tcp_new();
  assert_create_socket_succeeds();
}

Test(transport_mapper_inet, test_open_socket_fails_properly_on_socket_failure)
{
  transport_mapper = transport_mapper_tcp_new();
  transport_mapper_set_address_family(transport_mapper, 0xdeadbeef);
  assert_create_socket_fails();
}

Test(transport_mapper_inet, test_open_socket_fails_properly_on_bind_failure)
{
  transport_mapper = transport_mapper_tcp_new();
  GSockAddr *addr = g_sockaddr_unix_new("/foo/bar/no/such/dir");
  transport_mapper_set_address_family(transport_mapper, AF_UNIX);
  assert_create_socket_fails_with_address(addr);
  g_sockaddr_unref(addr);
}

static void
setup(void)
{
  app_startup();
}

static void
teardown(void)
{
  transport_mapper_free(transport_mapper);
  app_shutdown();
}

TestSuite(transport_mapper_inet, .init = setup, .fini = teardown);
