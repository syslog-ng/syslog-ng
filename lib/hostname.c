/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2013 Balázs Scheidler
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

#include "hostname.h"

#include <string.h>
#include <netdb.h>

static gsize local_hostname_fqdn_len;
static gchar local_hostname_fqdn[256];
static gsize local_hostname_short_len;
static gchar local_hostname_short[256];
G_LOCK_EXTERN(resolv_lock);

void
reset_cached_hostname(void)
{
  gchar *s;

  gethostname(local_hostname_fqdn, sizeof(local_hostname_fqdn) - 1);
  local_hostname_fqdn[sizeof(local_hostname_fqdn) - 1] = '\0';
  local_hostname_fqdn_len = strlen(local_hostname_fqdn);
  if (strchr(local_hostname_fqdn, '.') == NULL)
    {
      /* not fully qualified, resolve it using DNS or /etc/hosts */
      G_LOCK(resolv_lock);
      struct hostent *result = gethostbyname(local_hostname_fqdn);
      if (result)
        {
          strncpy(local_hostname_fqdn, result->h_name, sizeof(local_hostname_fqdn) - 1);
          local_hostname_fqdn[sizeof(local_hostname_fqdn) - 1] = '\0';
        }
      G_UNLOCK(resolv_lock);
    }
  /* NOTE: they are the same size, they'll fit */
  strcpy(local_hostname_short, local_hostname_fqdn);
  s = strchr(local_hostname_short, '.');
  if (s != NULL)
    *s = '\0';
  local_hostname_short_len = strlen(local_hostname_short);
}

const gchar *
get_local_hostname(gsize *len)
{
  if (!local_hostname_fqdn[0])
    reset_cached_hostname();

  if (len)
    *len = local_hostname_fqdn_len;
  return local_hostname_fqdn;
}
