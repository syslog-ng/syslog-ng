/*
 * Copyright (c) 2020 One Identity
 * Copyright (c) 2020 Antal Nemes
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

#include "autodetect-ca-location.h"
#include "glib/gstdio.h"

#include <unistd.h>

const gchar *
auto_detect_ca_file(void)
{
  static gchar *bundles[] =
  {
    "/etc/ssl/certs/ca-certificates.crt",
    "/etc/pki/tls/certs/ca-bundle.crt",
    "/usr/share/ssl/certs/ca-bundle.crt",
    "/usr/local/share/certs/ca-root-nss.crt",
    "/etc/ssl/cert.pem",
    NULL
  };

  for (int i = 0; bundles[i]; i++)
    {
      if (!g_access(bundles[i], R_OK))
        {
          return bundles[i];
        }
    }
  return NULL;
}

const gchar *
auto_detect_ca_dir(void)
{
  static gchar *ca_dirs[] =
  {
    "/etc/ssl/certs",
    NULL
  };

  for (int i = 0; ca_dirs[i]; i++)
    {
      if (!g_access(ca_dirs[i], R_OK))
        {
          return ca_dirs[i];
        }
    }

  return NULL;
}
