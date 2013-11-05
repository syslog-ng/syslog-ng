/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
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

/*
 * NOTE: this function is not thread safe because it uses the non-reentrant
 * resolver functions.  This is not a problem as it is only called once
 * during initialization.
 */
gchar *
get_local_fqdn_hostname_from_dns(void)
{
  struct hostent *host = NULL;
  gchar *result = NULL;
  gchar *local_host;

  local_host = get_local_hostname_from_system();

  host = gethostbyname(local_host);
  if (host)
     result = g_strdup(host->h_name);

  g_free(local_host);
  return result;
}
