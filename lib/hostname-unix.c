/*
 * Copyright (c) 2002-2013 Balabit
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
#include <netdb.h>

static struct hostent *
_resolve_localhost_from_dns(void)
{
  gchar *local_host;
  struct hostent *host;

  local_host = get_local_hostname_from_system();
  host = gethostbyname(local_host);
  g_free(local_host);

  return host;
}

static const gchar *
_extract_fqdn_from_hostent(struct hostent *host)
{
  gint i;

  if (is_hostname_fqdn(host->h_name))
    return host->h_name;

  for (i = 0; host->h_aliases[i]; i++)
    {
      if (is_hostname_fqdn(host->h_aliases[i]))
        return host->h_aliases[i];
    }
  return NULL;
}

/*
 * NOTE: this function is not thread safe because it uses the non-reentrant
 * resolver functions.  This is not a problem as it is only called once
 * during initialization when a single thread is active.
 */
gchar *
get_local_fqdn_hostname_from_dns(void)
{
  struct hostent *hostent;

  hostent = _resolve_localhost_from_dns();
  if (hostent)
    return g_strdup(_extract_fqdn_from_hostent(hostent));

  return NULL;
}
