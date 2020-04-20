/*
 * Copyright (c) 2002-2015 Balabit
 * Copyright (c) 1998-2015 Balázs Scheidler
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
#include "cfg.h"
#include "gsockaddr.h"
#include "messages.h"
#include "dnscache.h"

#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

static gchar local_hostname_fqdn[256];
static gchar local_hostname_short[256];
static gchar local_domain[256];
static gboolean local_domain_overridden;

static gchar *
get_local_hostname_from_system(void)
{
  gchar hostname[256];

  gethostname(hostname, sizeof(hostname) - 1);
  hostname[sizeof(hostname) - 1] = '\0';
  return g_strdup(hostname);
}

static gboolean
is_hostname_fqdn(const gchar *hostname)
{
  return strchr(hostname, '.') != NULL;
}

static const gchar *
extract_domain_from_fqdn(const gchar *hostname)
{
  const gchar *dot = strchr(hostname, '.');

  if (dot)
    return dot + 1;
  return NULL;
}

#include "hostname-unix.c"

static void
validate_hostname_cache(void)
{
  g_assert(local_hostname_fqdn[0] != 0);
}

const gchar *
get_local_hostname_fqdn(void)
{
  validate_hostname_cache();

  return local_hostname_fqdn;
}

const gchar *
get_local_hostname_short(void)
{
  validate_hostname_cache();
  return local_hostname_short;
}

gchar *
convert_hostname_to_fqdn(gchar *hostname, gsize hostname_len)
{
  gchar *end;

  /* we only change the hostname if domain_override is set. If it is unset,
   * our best guess is what we got as input.  */

  if (local_domain_overridden)
    convert_hostname_to_short_hostname(hostname, hostname_len);

  if (local_domain_overridden ||
      (!is_hostname_fqdn(hostname) && local_domain[0]))
    {
      end = hostname + strlen(hostname);
      if (end < hostname + hostname_len)
        {
          *end = '.';
          end++;
        }
      strncpy(end, local_domain, hostname_len - (end - hostname));
      hostname[hostname_len - 1] = 0;
    }
  return hostname;
}

gchar *
convert_hostname_to_short_hostname(gchar *hostname, gsize hostname_len)
{
  gchar *p;

  p = strchr(hostname, '.');
  if (p)
    *p = '\0';
  return hostname;
}

static void
detect_local_fqdn_hostname(void)
{
  gchar *hostname;

  hostname = get_local_hostname_from_system();
  if (!is_hostname_fqdn(hostname))
    {
      /* not fully qualified, resolve it using DNS or /etc/hosts */
      g_free(hostname);

      hostname = get_local_fqdn_hostname_from_dns();
      if (!hostname)
        {
          msg_verbose("Unable to detect fully qualified hostname for localhost, use_fqdn() will use the short hostname");
          hostname = get_local_hostname_from_system();
          if (!hostname[0])
            {
              msg_error("Could not resolve local hostname either from the DNS nor gethostname(), assuming localhost");
              hostname = g_strdup("localhost");
            }
        }
    }

  g_strlcpy(local_hostname_fqdn, hostname, sizeof(local_hostname_fqdn));
  g_free(hostname);
}

static void
detect_local_domain(void)
{
  const gchar *domain = extract_domain_from_fqdn(local_hostname_fqdn);

  if (domain)
    g_strlcpy(local_domain, domain, sizeof(local_domain));
  else
    local_domain[0] = 0;
}

static void
detect_local_short_hostname(void)
{
  g_strlcpy(local_hostname_short, local_hostname_fqdn, sizeof(local_hostname_short));
  convert_hostname_to_short_hostname(local_hostname_short, sizeof(local_hostname_short));
}

static void
set_domain_override(const gchar *domain_override)
{
  if (domain_override)
    {
      g_strlcpy(local_domain, domain_override, sizeof(local_domain));
      local_domain_overridden = TRUE;
    }
  else
    local_domain_overridden = FALSE;

  convert_hostname_to_fqdn(local_hostname_fqdn, sizeof(local_hostname_fqdn));
}

void
hostname_reinit(const gchar *domain_override)
{
  detect_local_fqdn_hostname();
  detect_local_domain();
  detect_local_short_hostname();
  set_domain_override(domain_override);
}

void
hostname_global_init(void)
{
  hostname_reinit(NULL);
}

void
hostname_global_deinit(void)
{
}
