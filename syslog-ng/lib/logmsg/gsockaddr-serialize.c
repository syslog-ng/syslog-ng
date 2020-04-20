/*
 * Copyright (c) 2002-2015 Balabit
 * Copyright (c) 2015 Viktor Juhasz <viktor.juhasz@balabit.com>
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

#include "gsockaddr-serialize.h"

#include <string.h>

static gboolean
_serialize_ipv4(GSockAddr *addr, SerializeArchive *sa)
{
  struct in_addr ina;
  ina = g_sockaddr_inet_get_address(addr);
  return serialize_write_blob(sa, (gchar *) &ina, sizeof(ina)) &&
         serialize_write_uint16(sa, htons(g_sockaddr_get_port(addr)));
}

#if SYSLOG_NG_ENABLE_IPV6
static gboolean
_serialize_ipv6(GSockAddr *addr, SerializeArchive *sa)
{
  struct in6_addr *in6a;
  in6a = g_sockaddr_inet6_get_address(addr);
  return serialize_write_blob(sa, (gchar *) in6a, sizeof(*in6a)) &&
         serialize_write_uint16(sa, htons(g_sockaddr_get_port(addr)));
}
#endif

gboolean
g_sockaddr_serialize(SerializeArchive *sa, GSockAddr *addr)
{
  if (!addr)
    {
      return serialize_write_uint16(sa, 0);
    }

  gboolean result = serialize_write_uint16(sa, addr->sa.sa_family);
  switch (addr->sa.sa_family)
    {
    case AF_INET:
    {
      result &= _serialize_ipv4(addr, sa);
      break;
    }
#if SYSLOG_NG_ENABLE_IPV6
    case AF_INET6:
    {
      result &= _serialize_ipv6(addr, sa);
      break;
    }
#endif
    case AF_UNIX:
    {
      break;
    }
    default:
    {
      result = FALSE;
      break;
    }
    }
  return result;
}

static gboolean
_deserialize_ipv4(SerializeArchive *sa, GSockAddr **addr)
{
  struct sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));

  sin.sin_family = AF_INET;
  if (!serialize_read_blob(sa, (gchar *) &sin.sin_addr, sizeof(sin.sin_addr)) ||
      !serialize_read_uint16(sa, &sin.sin_port))
    return FALSE;

  *addr = g_sockaddr_inet_new2(&sin);
  return TRUE;
}


#if SYSLOG_NG_ENABLE_IPV6
static gboolean
_deserialize_ipv6(SerializeArchive *sa, GSockAddr **addr)
{
  gboolean result = FALSE;
  struct sockaddr_in6 sin6;
  memset(&sin6, 0, sizeof(sin6));

  sin6.sin6_family = AF_INET6;
  if (serialize_read_blob(sa, (gchar *) &sin6.sin6_addr, sizeof(sin6.sin6_addr)) &&
      serialize_read_uint16(sa, &sin6.sin6_port))
    {
      *addr = g_sockaddr_inet6_new2(&sin6);
      result = TRUE;
    }
  return result;
}
#endif

gboolean
g_sockaddr_deserialize(SerializeArchive *sa, GSockAddr **addr)
{
  guint16 family;
  gboolean result = TRUE;

  if (!serialize_read_uint16(sa, &family))
    return FALSE;

  switch (family)
    {
    case 0:
      /* special case, no address was stored */
      *addr = NULL;
      break;
    case AF_INET:
    {
      result = _deserialize_ipv4(sa, addr);
      break;
    }
#if SYSLOG_NG_ENABLE_IPV6
    case AF_INET6:
    {
      result = _deserialize_ipv6(sa, addr);
      break;
    }
#endif
    case AF_UNIX:
      *addr = g_sockaddr_unix_new(NULL);
      break;
    default:
      result = FALSE;
      break;
    }
  return result;
}
