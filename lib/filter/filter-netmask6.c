/*
 * Copyright (c) 2014 BalaBit S.a.r.l., Luxembourg, Luxembourg
 * Copyright (c) 2014 Zoltan Fried
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

#include "gsocket.h"
#include "logmsg.h"

#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include "filter-netmask6.h"

typedef struct _FilterNetmask6
{
  FilterExprNode super;
  struct in6_addr address;
  int prefix;
} FilterNetmask6;

enum
{
  NMF6_PARSE_OK = 1,
  NMF6_ADDRESS_ERROR,
  NMF6_PREFIX_ERROR,
};

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
filter_netmask6_calc_network_address(unsigned char *ipv6, int prefix, struct in6_addr *network)
{
  struct ipv6_parts
  {
    uint64_t hi;
    uint64_t lo;
  } ipv6_parts;

  int length;

  memcpy(&ipv6_parts, ipv6, sizeof(ipv6_parts));
  if (prefix <= 64)
    {
      ipv6_parts.hi = _mask(ipv6_parts.hi, _calculate_mask_by_prefix(prefix));
      length = sizeof(uint64_t);
    }
  else
    {
      ipv6_parts.lo = _mask(ipv6_parts.lo, _calculate_mask_by_prefix(prefix - 64));
      length = 2 * sizeof(uint64_t);
    }

  memcpy(network->s6_addr, &ipv6_parts, length);
}

static inline gboolean
_in6_addr_equals(const struct in6_addr* address1, const struct in6_addr* address2)
{
  return memcmp(address1, address2, sizeof(struct in6_addr)) == 0;
}

static gboolean
_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg)
{
  FilterNetmask6 *self = (FilterNetmask6 *) s;
  LogMessage *msg = msgs[0];
  gboolean result = FALSE;
  struct in6_addr network_address;
  struct in6_addr address;

  if (msg->saddr && g_sockaddr_inet6_check(msg->saddr))
    {
      address = ((struct sockaddr_in6 *) &msg->saddr->sa)->sin6_addr;
    }
  else if (!msg->saddr || msg->saddr->sa.sa_family == AF_UNIX)
    {
      address = in6addr_loopback;
    }
  else
    {
      return s->comp;
    }

  memset(&network_address, 0, sizeof(struct in6_addr));
  filter_netmask6_calc_network_address((unsigned char *) &address, self->prefix, &network_address);
  result = _in6_addr_equals(&network_address, &self->address);

  return result ^ s->comp;
}

static gboolean
_get_slash_position(gchar *cidr, gint *slash_position)
{
  const char* slash = strchr(cidr, '/');
  if (slash)
    {
      *slash_position = slash - cidr;
      return TRUE;
    }
  return FALSE;
}

static inline gboolean
_is_valid_address_len(gint len)
{
  return (len > 0) && (len <= INET6_ADDRSTRLEN);
}

static inline gboolean
_is_valid_prefix(gint prefix)
{
 return prefix >=1 && prefix <= 128;
}

static gboolean
_parse_prefix(gchar *cidr, gint *prefix)
{
  const gint default_prefix = 128;
  gint slash_position;

  if (!_get_slash_position(cidr, &slash_position))
    {
      *prefix = default_prefix;
      return TRUE;
    }

  *prefix = strtol(&cidr[slash_position + 1], NULL, 10);
  return _is_valid_prefix(*prefix);
}

static void
_set_address(gchar* cidr, gint len, gchar address[])
{
  strncpy(address, cidr, len);
  address[len] = 0;
}

static gint
_get_address_len(gchar *cidr)
{
  gint slash_position;

  if (!_get_slash_position(cidr, &slash_position))
    {
      return strlen(cidr);
    }
  return slash_position;

}

static gboolean
_parse_address(gchar *cidr, gchar address[])
{
  gint len = _get_address_len(cidr);

  if (!_is_valid_address_len(len))
    return FALSE;
  _set_address(cidr, len, address);
  return TRUE;
}

static gint
_parse(gchar *cidr, gchar address[], gint *prefix)
{
  if (!_parse_address(cidr, address))
    return NMF6_ADDRESS_ERROR;
  if (!_parse_prefix(cidr, prefix))
    return NMF6_PREFIX_ERROR;

  return NMF6_PARSE_OK;
}

FilterExprNode *
filter_netmask6_new(gchar *cidr)
{
  FilterNetmask6 *self = g_new0(FilterNetmask6, 1);
  gchar address[INET6_ADDRSTRLEN + 1] = {0};
  struct in6_addr packet_addr;
  gint prefix;

  filter_expr_node_init_instance(&self->super);

  if (strlen(cidr) == 0)
    {
      goto filter_netmask6_invalid_error;
    }

  switch (_parse(cidr, address, &prefix))
    {
      case NMF6_ADDRESS_ERROR:
        goto filter_netmask6_invalid_error;
      case NMF6_PREFIX_ERROR:
        goto filter_netmask6_invalid_error;
    }


  self->prefix = prefix;

  if (inet_pton(AF_INET6, address, &packet_addr) == 1)
    filter_netmask6_calc_network_address((unsigned char *) &packet_addr, self->prefix, &self->address);
  else
    goto filter_netmask6_invalid_error;

  self->super.eval = _eval;
  return &self->super;

filter_netmask6_invalid_error:
  g_free(self);
  return NULL;
}
