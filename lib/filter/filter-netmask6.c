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


#include "filter-netmask6.h"
#include "gsocket.h"
#include "logmsg/logmsg.h"

#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#if SYSLOG_NG_ENABLE_IPV6
typedef struct _FilterNetmask6
{
  FilterExprNode super;
  struct in6_addr address;
  int prefix;
  gboolean is_valid;
} FilterNetmask6;

static inline uint64_t
_calculate_mask_by_prefix(int prefix)
{
  return (uint64_t) (~0) << (64 - prefix);
}

static inline uint64_t
_mask(uint64_t base, uint64_t mask)
{
  if (G_BYTE_ORDER == G_BIG_ENDIAN)
    return base & mask;
  else
    return GUINT64_SWAP_LE_BE(GUINT64_SWAP_LE_BE(base) & mask);
}

void
get_network_address(const struct in6_addr *address, int prefix, struct in6_addr *network)
{
  struct ipv6_parts
  {
    uint64_t lo;
    uint64_t hi;
  } ipv6_parts;

  int length;

  memset(network, 0, sizeof(*network));
  memcpy(&ipv6_parts, address, sizeof(*address));
  if (prefix <= 64)
    {
      ipv6_parts.lo = _mask(ipv6_parts.lo, _calculate_mask_by_prefix(prefix));
      length = sizeof(uint64_t);
    }
  else
    {
      ipv6_parts.hi = _mask(ipv6_parts.hi, _calculate_mask_by_prefix(prefix - 64));
      length = 2 * sizeof(uint64_t);
    }

  memcpy(network->s6_addr, &ipv6_parts, length);
}

static inline gboolean
_in6_addr_compare(const struct in6_addr *address1, const struct in6_addr *address2)
{
  return memcmp(address1, address2, sizeof(struct in6_addr)) == 0;
}

static gboolean
_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg)
{
  FilterNetmask6 *self = (FilterNetmask6 *) s;
  LogMessage *msg = msgs[num_msg - 1];
  gboolean result = FALSE;
  struct in6_addr network_address;
  const struct in6_addr *address;

  if (!self->is_valid)
    return s->comp;

  if (msg->saddr && g_sockaddr_inet6_check(msg->saddr))
    {
      address = &((struct sockaddr_in6 *) &msg->saddr->sa)->sin6_addr;
    }
  else if (!msg->saddr || msg->saddr->sa.sa_family == AF_UNIX)
    {
      address = &in6addr_loopback;
    }
  else
    {
      address = NULL;
    }

  if (address)
    {
      get_network_address(address, self->prefix, &network_address);
      result = _in6_addr_compare(&network_address, &self->address);
    }
  else
    result = FALSE;

  msg_debug("  netmask6() evaluation result",
            filter_result_tag(result),
            evt_tag_inaddr6("msg_address", address),
            evt_tag_inaddr6("address", &self->address),
            evt_tag_int("prefix", self->prefix),
            evt_tag_printf("msg", "%p", msg));
  return result ^ s->comp;
}

FilterExprNode *
filter_netmask6_new(gchar *cidr)
{
  FilterNetmask6 *self = g_new0(FilterNetmask6, 1);
  struct in6_addr packet_addr;
  gchar address[INET6_ADDRSTRLEN] = "";
  gchar *slash = strchr(cidr, '/');

  filter_expr_node_init_instance(&self->super);
  if (strlen(cidr) >= INET6_ADDRSTRLEN + 5 || !slash)
    {
      strcpy(address, cidr);
      self->prefix = 128;
    }
  else
    {
      self->prefix = atol(slash + 1);
      if (self->prefix > 0 && self->prefix < 129)
        {
          strncpy(address, cidr, slash - cidr);
          address[slash - cidr] = 0;
        }
    }

  self->is_valid = ((strlen(address) > 0) && inet_pton(AF_INET6, address, &packet_addr) == 1);
  if (self->is_valid)
    get_network_address(&packet_addr, self->prefix, &self->address);
  else
    self->address = in6addr_loopback;

  self->super.eval = _eval;
  return &self->super;
}
#endif
