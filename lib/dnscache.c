/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#include "dnscache.h"
#include "messages.h"
#include "timeutils.h"
#include "tls-support.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include <iv_list.h>

typedef struct _DNSCacheEntry DNSCacheEntry;
typedef struct _DNSCacheKey DNSCacheKey;

struct _DNSCacheKey
{
  gint family;
  union
  {
    struct in_addr ip;
#if SYSLOG_NG_ENABLE_IPV6
    struct in6_addr ip6;
#endif
  } addr;
};

struct _DNSCacheEntry
{
  struct iv_list_head list;
  DNSCacheKey key;
  time_t resolved;
  gchar *hostname;
  gsize hostname_len;
  /* whether this entry is a positive (successful DNS lookup) or negative (failed DNS lookup, contains an IP address) match */
  gboolean positive;
};

struct _DNSCache
{
  GHashTable *cache;
  const DNSCacheOptions *options;
  struct iv_list_head cache_list;
  struct iv_list_head persist_list;
  gint persistent_count;
  time_t hosts_mtime;
  time_t hosts_checktime;
};



static gboolean
dns_cache_key_equal(DNSCacheKey *e1, DNSCacheKey *e2)
{
  if (e1->family == e2->family)
    {
      if ((e1->family == AF_INET && memcmp(&e1->addr.ip, &e2->addr.ip, sizeof(e1->addr.ip)) == 0))
        return TRUE;
#if SYSLOG_NG_ENABLE_IPV6
      if ((e1->family == AF_INET6 && memcmp(&e1->addr.ip6, &e2->addr.ip6, sizeof(e1->addr.ip6)) == 0))
        return TRUE;
#endif
    }
  return FALSE;
}

static guint
dns_cache_key_hash(DNSCacheKey *e)
{
  if (e->family == AF_INET)
    {
      return ntohl(e->addr.ip.s_addr);
    }
#if SYSLOG_NG_ENABLE_IPV6
  else if (e->family == AF_INET6)
    {
      guint32 *a32 = (guint32 *) &e->addr.ip6.s6_addr;
      return (0x80000000 | (a32[0] ^ a32[1] ^ a32[2] ^ a32[3]));
    }
#endif
  else
    {
      g_assert_not_reached();
      return 0;
    }
}

static void
dns_cache_entry_free(DNSCacheEntry *e)
{
  iv_list_del(&e->list);

  g_free(e->hostname);
  g_free(e);
}

static inline void
dns_cache_fill_key(DNSCacheKey *key, gint family, void *addr)
{
  key->family = family;
  switch (family)
    {
    case AF_INET:
      key->addr.ip = *(struct in_addr *) addr;
      break;
#if SYSLOG_NG_ENABLE_IPV6
    case AF_INET6:
      key->addr.ip6 = *(struct in6_addr *) addr;
      break;
#endif
    default:
      g_assert_not_reached();
      break;
    }
}

static void
dns_cache_store(DNSCache *self, gboolean persistent, gint family, void *addr, const gchar *hostname, gboolean positive)
{
  DNSCacheEntry *entry;
  guint hash_size;

  entry = g_new(DNSCacheEntry, 1);

  dns_cache_fill_key(&entry->key, family, addr);
  entry->hostname = g_strdup(hostname);
  entry->hostname_len = strlen(hostname);
  entry->positive = positive;
  INIT_IV_LIST_HEAD(&entry->list);
  if (!persistent)
    {
      entry->resolved = cached_g_current_time_sec();
      iv_list_add(&entry->list, &self->cache_list);
    }
  else
    {
      entry->resolved = 0;
      iv_list_add(&entry->list, &self->persist_list);
    }
  hash_size = g_hash_table_size(self->cache);
  g_hash_table_replace(self->cache, &entry->key, entry);

  if (persistent && hash_size != g_hash_table_size(self->cache))
    self->persistent_count++;

  /* persistent elements are not counted */
  if ((gint) (g_hash_table_size(self->cache) - self->persistent_count) > self->options->cache_size)
    {
      DNSCacheEntry *entry_to_remove = iv_list_entry(self->cache_list.next, DNSCacheEntry, list);

      /* remove oldest element */
      g_hash_table_remove(self->cache, &entry_to_remove->key);
    }
}

void
dns_cache_store_persistent(DNSCache *self, gint family, void *addr, const gchar *hostname)
{
  dns_cache_store(self, TRUE, family, addr, hostname, TRUE);
}

void
dns_cache_store_dynamic(DNSCache *self, gint family, void *addr, const gchar *hostname, gboolean positive)
{
  dns_cache_store(self, FALSE, family, addr, hostname, positive);
}

static void
dns_cache_cleanup_persistent_hosts(DNSCache *self)
{
  struct iv_list_head *ilh, *ilh2;

  iv_list_for_each_safe(ilh, ilh2, &self->persist_list)
  {
    DNSCacheEntry *entry = iv_list_entry(ilh, DNSCacheEntry, list);

    g_hash_table_remove(self->cache, &entry->key);
    self->persistent_count--;
  }
}

static void
dns_cache_check_hosts(DNSCache *self, glong t)
{
  struct stat st;

  if (G_LIKELY(self->hosts_checktime == t))
    return;

  self->hosts_checktime = t;

  if (!self->options->hosts || stat(self->options->hosts, &st) < 0)
    {
      dns_cache_cleanup_persistent_hosts(self);
      return;
    }

  if (self->hosts_mtime == -1 || st.st_mtime > self->hosts_mtime)
    {
      FILE *hosts;

      self->hosts_mtime = st.st_mtime;
      dns_cache_cleanup_persistent_hosts(self);
      hosts = fopen(self->options->hosts, "r");
      if (hosts)
        {
          gchar buf[4096];
          char *strtok_saveptr;

          while (fgets(buf, sizeof(buf), hosts))
            {
              gchar *p, *ip;
              gint len;
              gint family;
              union
              {
                struct in_addr ip4;
#if SYSLOG_NG_ENABLE_IPV6
                struct in6_addr ip6;
#endif
              } ia;

              if (buf[0] == 0 || buf[0] == '\n' || buf[0] == '#')
                continue;

              len = strlen(buf);
              if (buf[len - 1] == '\n')
                buf[len-1] = 0;

              p = strtok_r(buf, " \t", &strtok_saveptr);
              if (!p)
                continue;
              ip = p;

#if SYSLOG_NG_ENABLE_IPV6
              if (strchr(ip, ':') != NULL)
                family = AF_INET6;
              else
#endif
                family = AF_INET;

              p = strtok_r(NULL, " \t", &strtok_saveptr);
              if (!p)
                continue;
              inet_pton(family, ip, &ia);
              dns_cache_store_persistent(self, family, &ia, p);
            }
          fclose(hosts);
        }
      else
        {
          msg_error("Error loading dns cache hosts file",
                    evt_tag_str("filename", self->options->hosts),
                    evt_tag_error("error"));
        }

    }
}

/*
 * @hostname        is set to the stored hostname,
 * @positive        is set whether the match was a DNS match or failure
 *
 * Returns TRUE if the cache was able to serve the request (e.g. had a
 * matching entry at all).
 */
gboolean
dns_cache_lookup(DNSCache *self, gint family, void *addr, const gchar **hostname, gsize *hostname_len,
                 gboolean *positive)
{
  DNSCacheKey key;
  DNSCacheEntry *entry;
  time_t now;

  now = cached_g_current_time_sec();
  dns_cache_check_hosts(self, now);

  dns_cache_fill_key(&key, family, addr);
  entry = g_hash_table_lookup(self->cache, &key);
  if (entry)
    {
      if (entry->resolved &&
          ((entry->positive && entry->resolved < now - self->options->expire) ||
           (!entry->positive && entry->resolved < now - self->options->expire_failed)))
        {
          /* the entry is not persistent and is too old */
        }
      else
        {
          *hostname = entry->hostname;
          *hostname_len = entry->hostname_len;
          *positive = entry->positive;
          return TRUE;
        }
    }
  *hostname = NULL;
  *positive = FALSE;
  return FALSE;
}

DNSCache *
dns_cache_new(const DNSCacheOptions *options)
{
  DNSCache *self = g_new0(DNSCache, 1);

  self->cache = g_hash_table_new_full((GHashFunc) dns_cache_key_hash, (GEqualFunc) dns_cache_key_equal, NULL,
                                      (GDestroyNotify) dns_cache_entry_free);
  INIT_IV_LIST_HEAD(&self->cache_list);
  INIT_IV_LIST_HEAD(&self->persist_list);
  self->hosts_mtime = -1;
  self->hosts_checktime = 0;
  self->persistent_count = 0;
  self->options = options;
  return self;
}

void
dns_cache_free(DNSCache *self)
{
  g_hash_table_destroy(self->cache);
  g_free(self);
}

void
dns_cache_options_defaults(DNSCacheOptions *options)
{
  options->cache_size = 1007;
  options->expire = 3600;
  options->expire_failed = 60;
  options->hosts = NULL;
}

void
dns_cache_options_destroy(DNSCacheOptions *options)
{
  g_free(options->hosts);
  options->hosts = NULL;
}

/**************************************************************************
 * The global API that manages DNSCache instances on its own. Callers need
 * not be aware of underlying data structures and locking, they can simply
 * call these functions to lookup/query the DNS cache.
 *
 * I would prefer these to be moved into a higher level implementation
 * detail.
 **************************************************************************/

TLS_BLOCK_START
{
  DNSCache *dns_cache;
}
TLS_BLOCK_END;

#define dns_cache __tls_deref(dns_cache)

/* DNS cache related options are global, independent of the configuration
 * (e.g.  GlobalConfig instance), and they are stored in the
 * "effective_dns_cache_options" variable below.
 *
 * The reasons for using the global variable are as explained below.
 *
 * Some notes:
 *   1) DNS cache contents are better retained between configuration reloads
 *   2) There are multiple DNSCache instances as they are per-thread data structs.
 *
 * The usual pattern would be:
 *    DNSCache->options -> DNSCacheOptions
 *
 * And options would be a reference and would point inside a DNSCacheOptions
 * instance contained within the GlobalConfig.
 *
 * The problem with this approach is that we don't want to recreate DNSCache
 * instances when reloading the configuration (as we want to keep their
 * config), and this would mean that we'd have to update the "options"
 * pointers in each of the existing instances.  And those instances are
 * per-thread variables, making it a bit more difficult to track them.
 *
 * For this reason, it was a lot simpler to use a global variable to hold
 * configuration options, one that can be updated as the configuration is
 * reloaded.  Then DNSCache instances transparently take the options changes
 * into account as they continue to resolve names.
 *
 * I could come up with even better solutions to this problem (like using a
 * layer above GlobalConfig, a state that encapsulates per-execution state
 * of syslog-ng), however, right now that would be an overkill and I want to
 * get the DNSCache refactors into the master tree.
 */

static DNSCacheOptions effective_dns_cache_options;
G_LOCK_DEFINE_STATIC(unused_dns_caches);
static GList *unused_dns_caches;

gboolean
dns_caching_lookup(gint family, void *addr, const gchar **hostname, gsize *hostname_len, gboolean *positive)
{
  return dns_cache_lookup(dns_cache, family, addr, hostname, hostname_len, positive);
}

void
dns_caching_store(gint family, void *addr, const gchar *hostname, gboolean positive)
{
  dns_cache_store_dynamic(dns_cache, family, addr, hostname, positive);
}

void
dns_caching_update_options(const DNSCacheOptions *new_options)
{
  DNSCacheOptions *options = &effective_dns_cache_options;

  if (options->hosts)
    g_free(options->hosts);

  options->cache_size = new_options->cache_size;
  options->expire = new_options->expire;
  options->expire_failed = new_options->expire_failed;
  options->hosts = g_strdup(new_options->hosts);
}

void
dns_caching_thread_init(void)
{
  g_assert(dns_cache == NULL);

  G_LOCK(unused_dns_caches);
  if (unused_dns_caches)
    {
      dns_cache = unused_dns_caches->data;
      unused_dns_caches = g_list_delete_link(unused_dns_caches, unused_dns_caches);
    }
  G_UNLOCK(unused_dns_caches);

  if (!dns_cache)
    dns_cache = dns_cache_new(&effective_dns_cache_options);
}

void
dns_caching_thread_deinit(void)
{
  g_assert(dns_cache != NULL);

  G_LOCK(unused_dns_caches);
  unused_dns_caches = g_list_prepend(unused_dns_caches, dns_cache);
  G_UNLOCK(unused_dns_caches);
  dns_cache = NULL;
}

void
dns_caching_global_init(void)
{
  dns_cache_options_defaults(&effective_dns_cache_options);
}

void
dns_caching_global_deinit(void)
{
  G_LOCK(unused_dns_caches);
  g_list_foreach(unused_dns_caches, (GFunc) dns_cache_free, NULL);
  g_list_free(unused_dns_caches);
  unused_dns_caches = NULL;
  G_UNLOCK(unused_dns_caches);
  dns_cache_options_destroy(&effective_dns_cache_options);
}
