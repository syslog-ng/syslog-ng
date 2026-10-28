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
#include "afsocket-dest.h"
#include "logqueue-fifo.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>

guint SCS_TCP;
guint SCS_TCP6;
guint SCS_UDP;
guint SCS_UDP6;
guint SCS_NETWORK;
guint SCS_SYSLOG;

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
  result = transport_mapper_open_socket(transport_mapper, &sock_options->super, addr, addr, AFSOCKET_DIR_RECV, sock);
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

#if ! defined(__FreeBSD__) // FreeBSD does not validate the matching of socket type and address family in socket() only in bind()

static void
assert_create_socket_fails_with_address(GSockAddr *addr)
{
  gint sock;

  cr_assert_not(create_socket_with_address(addr, &sock), "transport_mapper_open_socket() succeeded unexpectedly");
  cr_assert_eq(sock, -1, "failed create_socket returned a non-extremal value on failure");
}
#endif

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
  assert_transport_mapper_transport_name(transport_mapper, "rfc3164+tcp");
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
  assert_transport_mapper_transport_name(transport_mapper, "rfc3164+tcp");
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
  assert_transport_mapper_transport_name(transport_mapper, "rfc3164+udp");
}

Test(transport_mapper_inet, test_udp_apply_fails_when_tls_context_is_set)
{
  transport_mapper = transport_mapper_udp_new();
  transport_mapper_inet_set_tls_context((TransportMapperInet *) transport_mapper, create_dummy_tls_context());
  assert_transport_mapper_apply_fails(transport_mapper, "udp");
  assert_transport_mapper_transport_name(transport_mapper, "rfc3164+udp");
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
  assert_transport_mapper_transport_name(transport_mapper, "rfc3164+udp");
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
  assert_transport_mapper_transport_name(transport_mapper, "rfc3164+udp");
}

Test(transport_mapper_inet, test_network_transport_udp_apply_fails_when_tls_context_is_set)
{
  transport_mapper = transport_mapper_network_new();
  transport_mapper_inet_set_tls_context((TransportMapperInet *) transport_mapper, create_dummy_tls_context());
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
  assert_transport_mapper_transport_name(transport_mapper, "rfc3164+tcp");
}

Test(transport_mapper_inet, test_network_transport_tls_apply_fails_without_tls_context)
{
  transport_mapper = transport_mapper_network_new();
  assert_transport_mapper_apply_fails(transport_mapper, "tls");
}

Test(transport_mapper_inet, test_network_transport_tls_apply_transport_sets_defaults)
{
  transport_mapper = transport_mapper_network_new();
  transport_mapper_inet_set_tls_context((TransportMapperInet *) transport_mapper, create_dummy_tls_context());
  assert_transport_mapper_apply(transport_mapper, "tls");
  assert_transport_mapper_tcp_socket(transport_mapper);

  assert_transport_mapper_transport(transport_mapper, "tls");
  assert_transport_mapper_logproto(transport_mapper, "text");
  assert_transport_mapper_stats_source(transport_mapper, SCS_NETWORK);
  assert_transport_mapper_inet_server_port(transport_mapper, 514);
  assert_transport_mapper_transport_name(transport_mapper, "rfc3164+tls");
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
  assert_transport_mapper_transport_name(transport_mapper, "rfc3164+foo");
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
  assert_transport_mapper_transport_name(transport_mapper, "rfc5426");
}

Test(transport_mapper_inet, test_syslog_transport_udp_apply_fails_when_tls_context_is_set)
{
  transport_mapper = transport_mapper_syslog_new();
  transport_mapper_inet_set_tls_context((TransportMapperInet *) transport_mapper, create_dummy_tls_context());
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
  assert_transport_mapper_transport_name(transport_mapper, "rfc6587");
}

Test(transport_mapper_inet, test_syslog_transport_tls_apply_fails_without_tls_context)
{
  transport_mapper = transport_mapper_syslog_new();
  assert_transport_mapper_apply_fails(transport_mapper, "tls");
}

Test(transport_mapper_inet, test_syslog_transport_tls_apply_transport_sets_defaults)
{
  transport_mapper = transport_mapper_syslog_new();
  transport_mapper_inet_set_tls_context((TransportMapperInet *) transport_mapper, create_dummy_tls_context());
  assert_transport_mapper_apply(transport_mapper, "tls");
  assert_transport_mapper_tcp_socket(transport_mapper);

  assert_transport_mapper_transport(transport_mapper, "tls");
  assert_transport_mapper_logproto(transport_mapper, "framed");
  assert_transport_mapper_stats_source(transport_mapper, SCS_SYSLOG);
  assert_transport_mapper_inet_server_port(transport_mapper, 6514);
  assert_transport_mapper_transport_name(transport_mapper, "rfc5425");
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
  assert_transport_mapper_transport_name(transport_mapper, "rfc5424+foo");
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

#if ! defined(__FreeBSD__) // FreeBSD does not validate the matching of socket type and address family in socket() only in bind()

Test(transport_mapper_inet, test_open_socket_fails_properly_on_bind_failure)
{
  transport_mapper = transport_mapper_tcp_new();
  GSockAddr *addr = g_sockaddr_unix_new("/foo/bar/no/such/dir");
  transport_mapper_set_address_family(transport_mapper, AF_UNIX);
  assert_create_socket_fails_with_address(addr);
  g_sockaddr_unref(addr);
}
#endif

static gint reconnect_notifications;
static gint (*destination_notify)(LogPipe *, gint, gpointer);

static gint
_count_reconnect_notifications(LogPipe *pipe, gint code, gpointer user_data)
{
  /* Observe notifications without replacing the real destination's response. */
  if (code == NC_REOPEN_REQUIRED)
    reconnect_notifications++;
  return destination_notify(pipe, code, user_data);
}

static gboolean
_keep_reconnect_addresses(AFSocketDestDriver *driver)
{
  return TRUE;
}

static const gchar *
_reconnect_destination_name(const AFSocketDestDriver *driver)
{
  return "reconnect-regression";
}

static void
_stop_reconnect_loop(gpointer user_data)
{
  iv_quit();
}

/* Exercise a real TCP connection and afsocket callbacks, using an explicit
 * driver/writer fixture rather than full configuration startup. No active I/O
 * job is held here; deferred reopen is covered by the separate writer tests. */
Test(afsocket_reconnect, successful_reopen_does_not_create_a_second_connection, .timeout = 10)
{
  app_startup();
  configuration = cfg_new_snippet();
  gint listener = socket(AF_INET, SOCK_STREAM, 0);
  cr_assert_geq(listener, 0);
  struct sockaddr_in address =
  {
    .sin_family = AF_INET,
    .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
  };
  cr_assert_eq(bind(listener, (struct sockaddr *) &address, sizeof(address)), 0);
  cr_assert_eq(listen(listener, 4), 0);
  socklen_t address_len = sizeof(address);
  cr_assert_eq(getsockname(listener, (struct sockaddr *) &address, &address_len), 0);
  cr_assert_eq(fcntl(listener, F_SETFL, O_NONBLOCK), 0);

  /* Keep the ephemeral loopback addresses fixed, but use the actual transport
   * stack, connection establishment, writer, and destination notify handler. */
  AFSocketDestDriver *driver = g_new0(AFSocketDestDriver, 1);
  TransportMapper *mapper = transport_mapper_tcp_new();
  cr_assert(transport_mapper_apply_transport(mapper, configuration));
  SocketOptionsInet *socket_options = socket_options_inet_new_instance();
  afsocket_dd_init_instance(driver, &socket_options->super, mapper, configuration);
  driver->fd = -1;
  driver->setup_addresses = _keep_reconnect_addresses;
  driver->get_dest_name = _reconnect_destination_name;
  driver->bind_addr = g_sockaddr_inet_new("127.0.0.1", 0);
  driver->dest_addr = g_sockaddr_inet_new("127.0.0.1", ntohs(address.sin_port));
  driver->proto_factory = log_proto_client_get_factory(&configuration->plugin_context, "text");
  cr_assert_not_null(driver->proto_factory);
  log_writer_options_init(&driver->writer_options, configuration, LWO_NO_STATS);
  driver->writer_options.options &= ~LWO_THREADED;
  driver->writer_options.time_reopen = 1;
  driver->writer = log_writer_new(0, configuration);
  LogQueue *queue = log_queue_fifo_new(1000, NULL, STATS_LEVEL0, NULL, NULL);
  LogPipe *control = &driver->super.super.super;
  destination_notify = control->notify;
  control->notify = _count_reconnect_notifications;
  reconnect_notifications = 0;
  log_writer_set_options(driver->writer, control, &driver->writer_options, NULL, NULL);
  log_writer_set_queue(driver->writer, queue);
  cr_assert(log_pipe_init((LogPipe *) driver->writer));

  /* Arm the closed writer's retry and immediately initiate the real connection.
   * Successful establishment must cancel that retry. Three seconds covers its
   * one-second deadline plus the destination retry delay a stale notification
   * could schedule; correct behavior leaves no destination retry pending. */
  log_writer_reopen(driver->writer, NULL);
  afsocket_dd_reconnect(driver);
  struct iv_timer stop_timer;
  IV_TIMER_INIT(&stop_timer);
  stop_timer.handler = _stop_reconnect_loop;
  iv_validate_now();
  stop_timer.expires = iv_now;
  stop_timer.expires.tv_sec += 3;
  iv_timer_register(&stop_timer);
  iv_main();

  /* Drain the listener backlog only after the observation window, keeping the
   * server side open during the check so peer closure cannot trigger retries. */
  gint accepted_connections = 0;
  gint peer;
  while ((peer = accept(listener, NULL, NULL)) >= 0)
    {
      accepted_connections++;
      close(peer);
    }
  cr_assert(errno == EAGAIN || errno == EWOULDBLOCK);
  cr_assert_eq(accepted_connections, 1, "destination created a second TCP connection");
  cr_assert(log_writer_opened(driver->writer));
  /* The destination's opened-writer guard can prevent another connection even
   * with a stale notification, so connection count alone would mask that bug. */
  cr_assert_eq(reconnect_notifications, 0, "a stale writer timer reached the real destination handler");
  cr_assert_not(iv_timer_registered(&driver->reconnect_timer));
  cr_assert_not(iv_fd_registered(&driver->connect_fd));

  afsocket_dd_stop_watches(driver);
  cr_assert(log_pipe_deinit((LogPipe *) driver->writer));
  log_pipe_unref(control);
  log_queue_unref(queue);
  close(listener);
  cfg_free(configuration);
  app_shutdown();
  iv_deinit();
}

static void
setup(void)
{
  app_startup();
  SCS_TCP = stats_register_type("tcp");
  SCS_TCP6 = stats_register_type("tcp6");
  SCS_UDP = stats_register_type("udp");
  SCS_UDP6 = stats_register_type("udp6");
  SCS_NETWORK = stats_register_type("network");
  SCS_SYSLOG = stats_register_type("syslog");
}

static void
teardown(void)
{
  transport_mapper_free(transport_mapper);
  app_shutdown();
}

TestSuite(transport_mapper_inet, .init = setup, .fini = teardown);
