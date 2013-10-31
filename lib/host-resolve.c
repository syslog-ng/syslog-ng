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
#include "host-resolve.h"
#include "hostname.h"
#include "dnscache.h"
#include "messages.h"
#include "cfg.h"

#include <arpa/inet.h>
#include <netdb.h>

#if !defined(HAVE_GETADDRINFO) || !defined(HAVE_GETNAMEINFO)
G_LOCK_DEFINE_STATIC(resolv_lock);
#endif

static void
normalize_hostname(gchar *result, gsize result_size, const gchar *hostname)
{
  gsize i;

  for (i = 0; hostname[i] && i < (result_size - 1); i++)
    {
      result[i] = g_ascii_tolower(hostname[i]);
    }
  result[i] = '\0'; /* the closing \0 is not copied by the previous loop */
}

gboolean
resolve_hostname_to_sockaddr(GSockAddr **addr, gint family, const gchar *name)
{
  if (!name || name[0] == 0)
    {
      union
      {
#ifdef HAVE_STRUCT_SOCKADDR_STORAGE
        struct sockaddr_storage __sas;
#endif
        struct sockaddr_in __sin;
        struct sockaddr __sa;
      } sas;

      /* return the wildcard address that can be used as a bind address */
      memset(&sas, 0, sizeof(sas));
      sas.__sa.sa_family = family;
      switch (family)
        {
        case AF_INET:
          *addr = g_sockaddr_inet_new2(((struct sockaddr_in *) &sas));
          break;
#if ENABLE_IPV6
        case AF_INET6:
          *addr = g_sockaddr_inet6_new2((struct sockaddr_in6 *) &sas);
          break;
#endif
        default:
          g_assert_not_reached();
          break;
        }
      return TRUE;
    }

#ifdef HAVE_GETADDRINFO
  struct addrinfo hints;
  struct addrinfo *res;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = family;
  hints.ai_socktype = 0;
  hints.ai_protocol = 0;

  if (getaddrinfo(name, NULL, &hints, &res) == 0)
    {
      /* we only use the first entry in the returned list */
      switch (family)
        {
        case AF_INET:
          *addr = g_sockaddr_inet_new2(((struct sockaddr_in *) res->ai_addr));
          break;
#if ENABLE_IPV6
        case AF_INET6:
          *addr = g_sockaddr_inet6_new2((struct sockaddr_in6 *) res->ai_addr);
          break;
#endif
        default:
          g_assert_not_reached();
          break;
        }
      freeaddrinfo(res);
    }
  else
    {
      msg_error("Error resolving hostname",
                evt_tag_str("host", name),
                NULL);
      return FALSE;
    }
#else
  struct hostent *he;

  G_LOCK(resolv_lock);
  he = gethostbyname(name);
  if (he)
    {
      switch (family)
        {
        case AF_INET:
          {
            struct sockaddr_in sin;

            sin.sin_family = AF_INET;
            sin.sin_addr = *(struct in_addr *) he->h_addr;
            sin.sin_port = htons(0);
            *addr = g_sockaddr_inet_new2(&sin);
            break;
          }
        default:
          g_assert_not_reached();
          break;
        }
      G_UNLOCK(resolv_lock);
    }
  else
    {
      G_UNLOCK(resolv_lock);
      msg_error("Error resolving hostname",
                evt_tag_str("host", name),
                NULL);
      return FALSE;
    }
#endif
  return TRUE;
}

void
resolve_sockaddr_to_hostname(gchar *result, gsize result_size, gsize *result_len, GSockAddr *saddr, const HostResolveOptions *host_resolve_options)
{
  const gchar *hname;
  gboolean positive;
  gchar buf[256];

  if (saddr && saddr->sa.sa_family != AF_UNIX)
    {
      if (saddr->sa.sa_family == AF_INET
#if ENABLE_IPV6
          || saddr->sa.sa_family == AF_INET6
#endif
         )
        {
          void *addr;
          socklen_t addr_len G_GNUC_UNUSED;

          if (saddr->sa.sa_family == AF_INET)
            {
              addr = &((struct sockaddr_in *) &saddr->sa)->sin_addr;
              addr_len = sizeof(struct in_addr);
            }
#if ENABLE_IPV6
          else
            {
              addr = &((struct sockaddr_in6 *) &saddr->sa)->sin6_addr;
              addr_len = sizeof(struct in6_addr);
            }
#endif

          hname = NULL;
          if (host_resolve_options->use_dns)
            {
              if ((!host_resolve_options->use_dns_cache || !dns_cache_lookup(saddr->sa.sa_family, addr, (const gchar **) &hname, &positive)) && host_resolve_options->use_dns != 2)
                {
#ifdef HAVE_GETNAMEINFO
                  if (getnameinfo(&saddr->sa, saddr->salen, buf, sizeof(buf), NULL, 0, NI_NAMEREQD) == 0)
                    hname = buf;
#else
                  struct hostent *hp;

                  G_LOCK(resolv_lock);
                  hp = gethostbyaddr(addr, addr_len, saddr->sa.sa_family);
                  if (hp && hp->h_name)
                    {
                      strncpy(buf, hp->h_name, sizeof(buf));
                      buf[sizeof(buf) - 1] = 0;
                      hname = buf;
                    }

                  G_UNLOCK(resolv_lock);
#endif

                  if (hname)
                    positive = TRUE;

                  if (host_resolve_options->use_dns_cache && hname)
                    {
                      /* resolution success, store this as a positive match in the cache */
                      dns_cache_store(FALSE, saddr->sa.sa_family, addr, hname, TRUE);
                    }
                }
            }

          if (!hname)
            {
              inet_ntop(saddr->sa.sa_family, addr, buf, sizeof(buf));
              hname = buf;
              if (host_resolve_options->use_dns_cache)
                dns_cache_store(FALSE, saddr->sa.sa_family, addr, hname, FALSE);
            }
          else
            {
              if (!host_resolve_options->use_fqdn && positive)
                {
                  /* we only truncate hostnames if they were positive
                   * matches (e.g. real hostnames and not IP
                   * addresses) */
                  const gchar *p;
                  p = strchr(hname, '.');

                  if (p)
                    {
                      if (p - hname > sizeof(buf))
                        p = &hname[sizeof(buf)] - 1;
                      memcpy(buf, hname, p - hname);
                      buf[p - hname] = 0;
                      hname = buf;
                    }
                }
            }
        }
      else
        {
          g_assert_not_reached();
        }
    }
  else
    {
      if (host_resolve_options->use_fqdn)
        {
          hname = get_local_hostname_fqdn();
        }
      else
        {
          hname = get_local_hostname_short();
        }
    }
  if (host_resolve_options->normalize_hostnames)
    {
      normalize_hostname(result, result_size, hname);
      *result_len = strlen(result);
    }
  else
    {
      gsize len = strlen(hname);

      if (result_size < len - 1)
        len = result_size - 1;
      memcpy(result, hname, len);
      result[len] = 0;
      *result_len = len;
    }
}

void
resolve_hostname_to_hostname(gchar *result, gsize result_size, gsize *result_len, const gchar *hostname, HostResolveOptions *options)
{
  g_strlcpy(result, hostname, result_size);
  if (options->use_fqdn)
    convert_hostname_to_fqdn(result, result_size);
  else
    convert_hostname_to_short_hostname(result, result_size);

  if (options->normalize_hostnames)
    normalize_hostname(result, result_size, result);
  if (result_len)
    *result_len = strlen(result);
}

void
host_resolve_options_defaults(HostResolveOptions *options)
{
  options->use_dns = -1;
  options->use_fqdn = -1;
  options->use_dns_cache = -1;
  options->normalize_hostnames = -1;
}

void
host_resolve_options_init(HostResolveOptions *options, GlobalConfig *cfg)
{
  if (options->use_dns == -1)
    options->use_dns = cfg->host_resolve_options.use_dns;
  if (options->use_fqdn == -1)
    options->use_fqdn = cfg->host_resolve_options.use_fqdn;
  if (options->use_dns_cache == -1)
    options->use_dns_cache = cfg->host_resolve_options.use_dns_cache;
  if (options->normalize_hostnames == -1)
    options->normalize_hostnames = cfg->host_resolve_options.normalize_hostnames;
}

void
host_resolve_options_destroy(HostResolveOptions *options)
{
}
