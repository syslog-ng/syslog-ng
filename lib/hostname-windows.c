/*
 * Copyright (c) 2025 One Identity
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

 
/* NOTE: this file is included directly into hostname.c so the set of
 * includes here only add system dependent headers and not the full set
 */
#ifdef _WIN32

#include "compat/socket.h"
#include <string.h>

static struct addrinfo *
_resolve_localhost_from_dns(void)
{
  char host[256] = {0};
  if (gethostname(host, sizeof(host)-1) != 0 || !host[0])
    return NULL;

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family   = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags    = AI_CANONNAME;

  struct addrinfo *res = NULL;
  if (getaddrinfo(host, NULL, &hints, &res) != 0)
    return NULL;
  return res; /* caller frees with freeaddrinfo() */
}

static gchar *
_extract_fqdn_from_addrinfo(struct addrinfo *res)
{
  /* 1) Prefer canonical name if it looks FQDN */
  if (res && res->ai_canonname && strchr(res->ai_canonname, '.'))
    return g_strdup(res->ai_canonname);

  /* 2) fallback: reverse lookup first address and require dot */
  if (res && res->ai_addr) {
    char fqdn[NI_MAXHOST] = {0};
    if (getnameinfo(res->ai_addr, (socklen_t)res->ai_addrlen,
                    fqdn, sizeof(fqdn), NULL, 0, NI_NAMEREQD) == 0 &&
        strchr(fqdn, '.'))
      return g_strdup(fqdn);
  }
  return NULL;
}

gchar *
get_local_fqdn_hostname_from_dns(void)
{
  struct addrinfo *res = _resolve_localhost_from_dns();
  if (!res) return NULL;

  gchar *ret = _extract_fqdn_from_addrinfo(res);
  freeaddrinfo(res);
  return ret; /* may be NULL; caller handles */
}

#endif