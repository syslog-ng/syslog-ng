/*
 * Copyright (c) 2014 Balabit
 * Copyright (c) 2014 Zoltan Fried
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

#include "gsocket.h"
#include "logmsg/logmsg.h"

#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include "testutils.h"
#include "filter/filter-netmask6.h"

static void
_replace_last_zero_with_wildcard(gchar *ipv6)
{
  if (!ipv6)
    return;
  gsize n = strlen(ipv6);
  if ((n >= 2) && (ipv6[n-2] == ':') && (ipv6[n-1] == '0'))
    ipv6[n-1] = ':';
}

gchar *
calculate_network6(const gchar *ipv6, int prefix, gchar *calculated_network)
{
  struct in6_addr network;
  struct in6_addr address;

  memset(&network, 0, sizeof(struct in6_addr));

  inet_pton(AF_INET6, ipv6, &address);
  get_network_address(&address, prefix, &network);
  inet_ntop(AF_INET6, &network, calculated_network, INET6_ADDRSTRLEN);
  _replace_last_zero_with_wildcard(calculated_network);
  return calculated_network;
}

void
assert_netmask6(const gchar *ipv6, gint prefix, gchar *expected_network)
{
  char error_msg[64];
  sprintf(error_msg, "prefix: %d", prefix);

  gchar *calculated_network = g_new0(char, INET6_ADDRSTRLEN);
  calculate_network6(ipv6, prefix, calculated_network);
  assert_string(calculated_network, expected_network, error_msg);
  g_free(calculated_network);
}

int
main(void)
{
  const gchar *ipv6 = "2001:db80:85a3:8d30:1319:8a2e:3700:7348";

  assert_netmask6(ipv6, 1,   "::");
  assert_netmask6(ipv6, 3,   "2000::");
  assert_netmask6(ipv6, 16,  "2001::");
  assert_netmask6(ipv6, 17,  "2001:8000::");
  assert_netmask6(ipv6, 18,  "2001:c000::");
  assert_netmask6(ipv6, 20,  "2001:d000::");
  assert_netmask6(ipv6, 21,  "2001:d800::");
  assert_netmask6(ipv6, 23,  "2001:da00::");
  assert_netmask6(ipv6, 24,  "2001:db00::");
  assert_netmask6(ipv6, 25,  "2001:db80::");
  assert_netmask6(ipv6, 33,  "2001:db80:8000::");
  assert_netmask6(ipv6, 38,  "2001:db80:8400::");
  assert_netmask6(ipv6, 40,  "2001:db80:8500::");
  assert_netmask6(ipv6, 41,  "2001:db80:8580::");
  assert_netmask6(ipv6, 43,  "2001:db80:85a0::");
  assert_netmask6(ipv6, 47,  "2001:db80:85a2::");
  assert_netmask6(ipv6, 48,  "2001:db80:85a3::");
  assert_netmask6(ipv6, 49,  "2001:db80:85a3:8000::");
  assert_netmask6(ipv6, 54,  "2001:db80:85a3:8c00::");
  assert_netmask6(ipv6, 56,  "2001:db80:85a3:8d00::");
  assert_netmask6(ipv6, 59,  "2001:db80:85a3:8d20::");
  assert_netmask6(ipv6, 60,  "2001:db80:85a3:8d30::");
  assert_netmask6(ipv6, 68,  "2001:db80:85a3:8d30:1000::");
  assert_netmask6(ipv6, 71,  "2001:db80:85a3:8d30:1200::");
  assert_netmask6(ipv6, 72,  "2001:db80:85a3:8d30:1300::");
  assert_netmask6(ipv6, 76,  "2001:db80:85a3:8d30:1310::");
  assert_netmask6(ipv6, 77,  "2001:db80:85a3:8d30:1318::");
  assert_netmask6(ipv6, 80,  "2001:db80:85a3:8d30:1319::");
  assert_netmask6(ipv6, 81,  "2001:db80:85a3:8d30:1319:8000::");
  assert_netmask6(ipv6, 87,  "2001:db80:85a3:8d30:1319:8a00::");
  assert_netmask6(ipv6, 91,  "2001:db80:85a3:8d30:1319:8a20::");
  assert_netmask6(ipv6, 93,  "2001:db80:85a3:8d30:1319:8a28::");
  assert_netmask6(ipv6, 94,  "2001:db80:85a3:8d30:1319:8a2c::");
  assert_netmask6(ipv6, 95,  "2001:db80:85a3:8d30:1319:8a2e::");
  assert_netmask6(ipv6, 99,  "2001:db80:85a3:8d30:1319:8a2e:2000::");
  assert_netmask6(ipv6, 100, "2001:db80:85a3:8d30:1319:8a2e:3000::");
  assert_netmask6(ipv6, 102, "2001:db80:85a3:8d30:1319:8a2e:3400::");
  assert_netmask6(ipv6, 103, "2001:db80:85a3:8d30:1319:8a2e:3600::");
  assert_netmask6(ipv6, 104, "2001:db80:85a3:8d30:1319:8a2e:3700::");
  assert_netmask6(ipv6, 114, "2001:db80:85a3:8d30:1319:8a2e:3700:4000");
  assert_netmask6(ipv6, 115, "2001:db80:85a3:8d30:1319:8a2e:3700:6000");
  assert_netmask6(ipv6, 116, "2001:db80:85a3:8d30:1319:8a2e:3700:7000");
  assert_netmask6(ipv6, 119, "2001:db80:85a3:8d30:1319:8a2e:3700:7200");
  assert_netmask6(ipv6, 120, "2001:db80:85a3:8d30:1319:8a2e:3700:7300");
  assert_netmask6(ipv6, 122, "2001:db80:85a3:8d30:1319:8a2e:3700:7340");
  assert_netmask6(ipv6, 125, "2001:db80:85a3:8d30:1319:8a2e:3700:7348");
  return 0;
}


