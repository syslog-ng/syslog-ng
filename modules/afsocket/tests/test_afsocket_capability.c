/*
 * Copyright (c) 2002-2015 BalaBit IT Ltd, Budapest, Hungary
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

#include "apphook.h"
#include "gprocess.h"
#include "testutils.h"
#include "cfg.h"
#include "mainloop.h"
#include "capability_lib.h"

#include "modules/afsocket/transport-mapper-inet.h"
#include "modules/afsocket/socket-options-inet.h"
#include "modules/afsocket/transport-mapper.h"

#define CAP_TESTCASE(testfunc, ...) { testcase_begin("%s(%s)", #testfunc, #__VA_ARGS__); testfunc(__VA_ARGS__); testcase_end(); }

#define config "destination d_net { network(\"127.0.0.1\" transport(\"udp\") spoof-source(yes) port(1222) ); }; log { destination(d_net); }; "


static void
afinet_dest(void)
{
  cap_counter_reset();
  configuration = cfg_new(VERSION_VALUE);
  configuration->threaded = FALSE;

  plugin_load_module("afsocket", configuration, NULL);
  main_thread_handle = get_thread_id();
  cfg_load_config(configuration, config, FALSE, NULL);

  assert_true(cfg_init(configuration), "Error in configuration");

  assert_true(check_cap_count(CAP_NET_BIND_SERVICE, 1, 1), "Cap not raised: CAP_NET_BIND_SERVICE");
  assert_true(check_cap_count(CAP_DAC_OVERRIDE, 1, 1), "Cap not raised: CAP_DAC_OVERRIDE");
#if ENABLE_SPOOF_SOURCE
  assert_true(check_cap_count(CAP_NET_RAW, 1, 1), "Cap not raised: CAP_NET_RAW");
#endif

  assert_true(check_cap_save(2), "Cap is not saved");
  assert_true(check_cap_restore(2), "Cap is not restored");

  cfg_deinit(configuration);
  cfg_free(configuration);
}

static void
g_process_cap_syslog_support(void)
{
  cap_counter_reset();
  g_process_capability->have_capsyslog = FALSE;
  assert_true(when_cap_syslog_is_not_supported_resort_to_cap_sys_admin(CAP_SYSLOG) == CAP_SYS_ADMIN, "CAP_SYSLOG is not resorted to CAP_SYSADMIN");
  assert_true(when_cap_syslog_is_not_supported_resort_to_cap_sys_admin(CAP_CHOWN) == CAP_CHOWN, "CAP_CHOWN is not resorted CAP_CHOWN");

  g_process_capability->have_capsyslog = TRUE;
  assert_true(when_cap_syslog_is_not_supported_resort_to_cap_sys_admin(CAP_SYSLOG) == CAP_SYSLOG, "CAP_SYSLOG is not resorted to CAP_SYSLOG");
  assert_true(when_cap_syslog_is_not_supported_resort_to_cap_sys_admin(CAP_CHOWN) == CAP_CHOWN, "CAP_CHOWN is not resorted CAP_CHOWN");
}

static gboolean
_create_socket_with_address(GSockAddr *addr, gint *sock)
{
  SocketOptionsInet *sock_options = socket_options_inet_new_instance();

  return transport_mapper_open_socket(transport_mapper_tcp_new(), &sock_options->super, addr, AFSOCKET_DIR_RECV, sock);
}

static gboolean
_create_socket(gint *sock)
{
  GSockAddr *addr = g_sockaddr_inet_new("127.0.0.1", 0);
  gboolean success;

  success = _create_socket_with_address(addr, sock);
  g_sockaddr_unref(addr);
  return success;
}

static void
transport_mapper_bind(void)
{
  gint sock;

  assert_true(_create_socket(&sock), "transport_mapper_open_socket() failed unexpectedly");

  assert_true(check_cap_count(CAP_NET_BIND_SERVICE, 1, 1), "Cap not raised: CAP_NET_BIND_SERVICE");
  assert_true(check_cap_count(CAP_DAC_OVERRIDE, 1, 1), "Cap not raised: CAP_DAC_OVERRIDE");

  assert_true(check_cap_save(1), "Cap is not saved");
  assert_true(check_cap_restore(1), "Cap is not restored");

  close(sock);
  cap_counter_reset();
}

int
main()
{
  app_startup();
  capability_mocking_reinit();

  CAP_TESTCASE(afinet_dest);
  CAP_TESTCASE(g_process_cap_syslog_support);
  CAP_TESTCASE(transport_mapper_bind);

  app_shutdown();
  return 0;
}
