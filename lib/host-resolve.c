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
#include "host-resolve.h"
#include "hostname.h"
#include "dnscache.h"
#include "messages.h"
#include "cfg.h"
#include "tls-support.h"
#include "compat/socket.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>

#if !defined(SYSLOG_NG_HAVE_GETADDRINFO) || !defined(SYSLOG_NG_HAVE_GETNAMEINFO)
G_LOCK_DEFINE_STATIC(resolv_lock);
#endif

TLS_BLOCK_START
{
  gchar hostname_buffer[256];
}
TLS_BLOCK_END;

#define hostname_buffer  __tls_deref(hostname_buffer)

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

static const gchar *
bounce_to_hostname_buffer(const gchar *hname)
{
  if (hname != hostname_buffer)
    g_strlcpy(hostname_buffer, hname, sizeof(hostname_buffer));
  return hostname_buffer;
}

static const gchar *
hostname_apply_options(gssize result_len_orig, gsize *result_len, const gchar *hname,
                       const HostResolveOptions *host_resolve_options)
{
  if (host_resolve_options->normalize_hostnames)
    {
      normalize_hostname(hostname_buffer, sizeof(hostname_buffer), hname);
      hname = hostname_buffer;
    }
  if (result_len_orig >= 0)
    *result_len = result_len_orig;
  else
    *result_len = strlen(hname);
  return hname;

}

static const gchar *
hostname_apply_options_fqdn(gssize result_len_orig, gsize *result_len, const gchar *hname, gboolean positive,
                            const HostResolveOptions *host_resolve_options)
{
  if (positive && !host_resolve_options->use_fqdn)
    {
      /* we only truncate hostnames if they were positive
       * matches (e.g. real hostnames and not IP
       * addresses) */

      hname = bounce_to_hostname_buffer(hname);
      convert_hostname_to_short_hostname(hostname_buffer, sizeof(hostname_buffer));
      result_len_orig = -1;
    }
  return hostname_apply_options(result_len_orig, result_len, hname, host_resolve_options);
}

/****************************************************************************
 * Convert a GSockAddr instance to a hostname
 ****************************************************************************/

static gboolean
is_wildcard_hostname(const gchar *name)
{
  return !name || name[0] == 0;
}

static gboolean
resolve_wildcard_hostname_to_sockaddr(GSockAddr **addr, gint family, const gchar *name)
{
  struct sockaddr_storage ss;

  /* return the wildcard address that can be used as a bind address */
  memset(&ss, 0, sizeof(ss));
  ss.ss_family = family;
  switch (family)
    {
    case AF_INET:
      *addr = g_sockaddr_inet_new2(((struct sockaddr_in *) &ss));
      break;
#if SYSLOG_NG_ENABLE_IPV6
    case AF_INET6:
      *addr = g_sockaddr_inet6_new2((struct sockaddr_in6 *) &ss);
      break;
#endif
    default:
      g_assert_not_reached();
      break;
    }
  return TRUE;
}

#ifdef SYSLOG_NG_HAVE_GETADDRINFO
static gboolean
resolve_hostname_to_sockaddr_using_getaddrinfo(GSockAddr **addr, gint family, const gchar *name)
{
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
#if SYSLOG_NG_ENABLE_IPV6
        case AF_INET6:
          *addr = g_sockaddr_inet6_new2((struct sockaddr_in6 *) res->ai_addr);
          break;
#endif
        default:
          g_assert_not_reached();
          break;
        }
      freeaddrinfo(res);
      return TRUE;
    }
  return FALSE;
}

#else

static gboolean
resolve_hostname_to_sockaddr_using_gethostbyname(GSockAddr **addr, gint family, const gchar *name)
{
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
    }
  G_UNLOCK(resolv_lock);
  return he != NULL;
}
#endif

gboolean
resolve_hostname_to_sockaddr(GSockAddr **addr, gint family, const gchar *name)
{
  gboolean result;

  if (is_wildcard_hostname(name))
    return resolve_wildcard_hostname_to_sockaddr(addr, family, name);

#ifdef SYSLOG_NG_HAVE_GETADDRINFO
  result = resolve_hostname_to_sockaddr_using_getaddrinfo(addr, family, name);
#else
  result = resolve_hostname_to_sockaddr_using_gethostbyname(addr, family, name);
#endif
  if (!result)
    {
      msg_error("Error resolving hostname",
                evt_tag_str("host", name));
    }
  return result;
}

/****************************************************************************
 * Convert a hostname to a GSockAddr instance
 ****************************************************************************/

static gboolean
is_sockaddr_local(GSockAddr *saddr)
{
  return !saddr || (saddr->sa.sa_family != AF_INET && saddr->sa.sa_family != AF_INET6);
}

static const gchar *
resolve_sockaddr_to_local_hostname(gsize *result_len, GSockAddr *saddr, const HostResolveOptions *host_resolve_options)
{
  const gchar *hname;

  if (host_resolve_options->use_fqdn)
    hname = get_local_hostname_fqdn();
  else
    hname = get_local_hostname_short();

  return hostname_apply_options(-1, result_len, hname, host_resolve_options);
}

#ifdef SYSLOG_NG_HAVE_GETNAMEINFO

static const gchar *
resolve_address_using_getnameinfo(GSockAddr *saddr, gchar *buf, gsize buf_len)
{
  if (getnameinfo(&saddr->sa, saddr->salen, buf, buf_len, NULL, 0, NI_NAMEREQD) == 0)
    return buf;
  return NULL;
}

#else

static const gchar *
resolve_address_using_gethostbyaddr(GSockAddr *saddr, gchar *buf, gsize buf_len)
{
  const gchar *result = NULL;
  struct hostent *hp;
  void *addr;
  socklen_t addr_len G_GNUC_UNUSED;

  g_assert(saddr->sa.sa_family == AF_INET);

  addr = &((struct sockaddr_in *) &saddr->sa)->sin_addr;
  addr_len = sizeof(struct in_addr);

  G_LOCK(resolv_lock);
  hp = gethostbyaddr(addr, addr_len, saddr->sa.sa_family);
  if (hp && hp->h_name)
    {
      strncpy(buf, hp->h_name, buf_len);
      buf[buf_len - 1] = 0;
      result = buf;
    }

  G_UNLOCK(resolv_lock);
  return result;
}

#endif

static void *
sockaddr_to_dnscache_key(GSockAddr *saddr)
{
  if (saddr->sa.sa_family == AF_INET)
    return &((struct sockaddr_in *) &saddr->sa)->sin_addr;
#if SYSLOG_NG_ENABLE_IPV6
  else if (saddr->sa.sa_family == AF_INET6)
    return &((struct sockaddr_in6 *) &saddr->sa)->sin6_addr;
#endif
  else
    {
      msg_warning("Socket address is neither IPv4 nor IPv6",
                  evt_tag_int("sa_family", saddr->sa.sa_family));
      return NULL;
    }
}

static const gchar *
resolve_sockaddr_to_inet_or_inet6_hostname(gsize *result_len, GSockAddr *saddr,
                                           const HostResolveOptions *host_resolve_options)
{
  const gchar *hname;
  gsize hname_len;
  gboolean positive;
  void *dnscache_key;

  dnscache_key = sockaddr_to_dnscache_key(saddr);

  hname = NULL;
  positive = FALSE;

  if (host_resolve_options->use_dns_cache)
    {
      if (dns_caching_lookup(saddr->sa.sa_family, dnscache_key, (const gchar **) &hname, &hname_len, &positive))
        return hostname_apply_options_fqdn(hname_len, result_len, hname, positive, host_resolve_options);
    }

  if (!hname && host_resolve_options->use_dns && host_resolve_options->use_dns != 2)
    {
#ifdef SYSLOG_NG_HAVE_GETNAMEINFO
      hname = resolve_address_using_getnameinfo(saddr, hostname_buffer, sizeof(hostname_buffer));
#else
      hname = resolve_address_using_gethostbyaddr(saddr, hostname_buffer, sizeof(hostname_buffer));
#endif
      positive = (hname != NULL);
    }

  if (!hname)
    {
      hname = g_sockaddr_format(saddr, hostname_buffer, sizeof(hostname_buffer), GSA_ADDRESS_ONLY);
      positive = FALSE;
    }
  if (host_resolve_options->use_dns_cache)
    dns_caching_store(saddr->sa.sa_family, dnscache_key, hname, positive);

  return hostname_apply_options_fqdn(-1, result_len, hname, positive, host_resolve_options);
}

const gchar *
resolve_sockaddr_to_hostname(gsize *result_len, GSockAddr *saddr, const HostResolveOptions *host_resolve_options)
{
  if (is_sockaddr_local(saddr))
    return resolve_sockaddr_to_local_hostname(result_len, saddr, host_resolve_options);
  else
    return resolve_sockaddr_to_inet_or_inet6_hostname(result_len, saddr, host_resolve_options);
}

/****************************************************************************
 * Convert a hostname to a hostname with options applied.
 ****************************************************************************/

const gchar *
resolve_hostname_to_hostname(gsize *result_len, const gchar *hname, HostResolveOptions *host_resolve_options)
{
  hname = bounce_to_hostname_buffer(hname);

  if (host_resolve_options->use_fqdn)
    convert_hostname_to_fqdn(hostname_buffer, sizeof(hostname_buffer));
  else
    convert_hostname_to_short_hostname(hostname_buffer, sizeof(hostname_buffer));

  return hostname_apply_options(-1, result_len, hname, host_resolve_options);
}

/****************************************************************************
 * HostResolveOptions
 ****************************************************************************/

void
host_resolve_options_defaults(HostResolveOptions *options)
{
  options->use_dns = -1;
  options->use_fqdn = -1;
  options->use_dns_cache = -1;
  options->normalize_hostnames = -1;
}

void
host_resolve_options_global_defaults(HostResolveOptions *options)
{
  options->use_fqdn = FALSE;
  options->use_dns = TRUE;
  options->use_dns_cache = TRUE;
  options->normalize_hostnames = FALSE;
}

static void
_init_options(HostResolveOptions *options)
{
}

void
host_resolve_options_init_globals(HostResolveOptions *options)
{
  _init_options(options);
}

void
host_resolve_options_init(HostResolveOptions *options, HostResolveOptions *global_options)
{
  if (options->use_dns == -1)
    options->use_dns = global_options->use_dns;
  if (options->use_fqdn == -1)
    options->use_fqdn = global_options->use_fqdn;
  if (options->use_dns_cache == -1)
    options->use_dns_cache = global_options->use_dns_cache;
  if (options->normalize_hostnames == -1)
    options->normalize_hostnames = global_options->normalize_hostnames;
  _init_options(options);
}

void
host_resolve_options_destroy(HostResolveOptions *options)
{
}
