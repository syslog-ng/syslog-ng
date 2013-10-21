/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
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

typedef struct _DNSCacheEntry DNSCacheEntry;
typedef struct _DNSCacheKey DNSCacheKey;

struct _DNSCacheKey
{
  gint family;
  union
  {
    struct in_addr ip;
#if ENABLE_IPV6
    struct in6_addr ip6;
#endif
  } addr;
};

struct _DNSCacheEntry
{
  DNSCacheEntry *prev, *next;
  DNSCacheKey key;
  time_t resolved;
  gchar *hostname;
  /* whether this entry is a positive (successful DNS lookup) or negative (failed DNS lookup, contains an IP address) match */
  gboolean positive;
};


TLS_BLOCK_START
{
  GHashTable *cache;
  DNSCacheEntry cache_first;
  DNSCacheEntry cache_last;
  DNSCacheEntry persist_first;
  DNSCacheEntry persist_last;
}
TLS_BLOCK_END;

#define cache  __tls_deref(cache)
#define cache_first __tls_deref(cache_first)
#define cache_last __tls_deref(cache_last)
#define persist_first __tls_deref(persist_first)
#define persist_last __tls_deref(persist_last)

static gint dns_cache_size = 1007;
static gint dns_cache_expire = 3600;
static gint dns_cache_expire_failed = 60;
static gint dns_cache_persistent_count = 0;
static gchar *dns_cache_hosts = NULL;
static time_t dns_cache_hosts_mtime = -1;
static time_t dns_cache_hosts_checktime = 0;

static gboolean 
dns_cache_key_equal(DNSCacheKey *e1, DNSCacheKey *e2)
{
  if (e1->family == e2->family)
    {
      if ((e1->family == AF_INET && memcmp(&e1->addr.ip, &e2->addr.ip, sizeof(e1->addr.ip)) == 0))
        return TRUE;
#if ENABLE_IPV6
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
#if ENABLE_IPV6
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

static inline void
dns_cache_entry_insert_before(DNSCacheEntry *elem, DNSCacheEntry *new_elem)
{
  elem->prev->next = new_elem;
  new_elem->prev = elem->prev;
  new_elem->next = elem;
  elem->prev = new_elem;
}

static void
dns_cache_entry_free(DNSCacheEntry *e)
{
  e->prev->next = e->next;
  e->next->prev = e->prev;
  
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
#if ENABLE_IPV6
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
dns_cache_cleanup_persistent_hosts(void)
{
  while (persist_first.next != &persist_last)
    {
      g_hash_table_remove(cache, &persist_first.next->key);
      dns_cache_persistent_count--;
    }
}

static void
dns_cache_check_hosts(glong t)
{
  struct stat st;

  if (G_LIKELY(dns_cache_hosts_checktime == t))
    return;

  dns_cache_hosts_checktime = t;
  
  if (!dns_cache_hosts || stat(dns_cache_hosts, &st) < 0)
    {
      dns_cache_cleanup_persistent_hosts();
      return;
    }
    
  if (dns_cache_hosts_mtime == -1 || st.st_mtime > dns_cache_hosts_mtime)
    {
      FILE *hosts;
      
      dns_cache_hosts_mtime = st.st_mtime;
      dns_cache_cleanup_persistent_hosts();
      hosts = fopen(dns_cache_hosts, "r");
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
#if ENABLE_IPV6
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

#if ENABLE_IPV6
              if (strchr(ip, ':') != NULL)
                family = AF_INET6;
              else
#endif
              family = AF_INET;
                
              p = strtok_r(NULL, " \t", &strtok_saveptr);
              if (!p)
                continue;
              inet_pton(family, ip, &ia);
              dns_cache_store(TRUE, family, &ia, p, TRUE);
            }
          fclose(hosts);
        }
      else
        {
          msg_error("Error loading dns cache hosts file", 
                    evt_tag_str("filename", dns_cache_hosts), 
                    evt_tag_errno("error", errno),
                    NULL);
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
dns_cache_lookup(gint family, void *addr, const gchar **hostname, gboolean *positive)
{
  DNSCacheKey key;
  DNSCacheEntry *entry;
  time_t now;
  
  now = cached_g_current_time_sec();
  dns_cache_check_hosts(now);
  
  dns_cache_fill_key(&key, family, addr);
  entry = g_hash_table_lookup(cache, &key);
  if (entry)
    {
      if (entry->resolved && 
          ((entry->positive && entry->resolved < now - dns_cache_expire) ||
           (!entry->positive && entry->resolved < now - dns_cache_expire_failed)))
        {
          /* the entry is not persistent and is too old */
        }
      else
        {
          *hostname = entry->hostname;
          *positive = entry->positive;
          return TRUE;
        }
    }
  *hostname = NULL;
  *positive = FALSE;
  return FALSE;
}

void
dns_cache_store(gboolean persistent, gint family, void *addr, const gchar *hostname, gboolean positive)
{
  DNSCacheEntry *entry;
  guint hash_size;
  
  entry = g_new(DNSCacheEntry, 1);

  dns_cache_fill_key(&entry->key, family, addr);
  entry->hostname = hostname ? g_strdup(hostname) : NULL;
  entry->positive = positive;
  if (!persistent)
    {
      entry->resolved = cached_g_current_time_sec();
      dns_cache_entry_insert_before(&cache_last, entry);
    }
  else
    {
      entry->resolved = 0;
      dns_cache_entry_insert_before(&persist_last, entry);
    }
  hash_size = g_hash_table_size(cache);
  g_hash_table_replace(cache, &entry->key, entry);

  if (persistent && hash_size != g_hash_table_size(cache))
    dns_cache_persistent_count++;
  
  /* persistent elements are not counted */
  if ((gint) (g_hash_table_size(cache) - dns_cache_persistent_count) > dns_cache_size)
    {
      /* remove oldest element */
      g_hash_table_remove(cache, &cache_first.next->key);
    }
}

void
dns_cache_set_params(gint cache_size, gint expire, gint expire_failed, const gchar *hosts)
{
  if (dns_cache_hosts)
    g_free(dns_cache_hosts);
    
  dns_cache_size = cache_size;
  dns_cache_expire = expire;
  dns_cache_expire_failed = expire_failed;
  dns_cache_hosts = g_strdup(hosts);
  dns_cache_hosts_mtime = -1;
  dns_cache_hosts_checktime = 0;
}

void
dns_cache_init(void)
{
  cache = g_hash_table_new_full((GHashFunc) dns_cache_key_hash, (GEqualFunc) dns_cache_key_equal, NULL, (GDestroyNotify) dns_cache_entry_free);
  cache_first.next = &cache_last;
  cache_first.prev = NULL;
  cache_last.prev = &cache_first;
  cache_last.next = NULL;

  persist_first.next = &persist_last;
  persist_first.prev = NULL;
  persist_last.prev = &persist_first;
  persist_last.next = NULL;
}

void
dns_cache_destroy(void)
{
  g_hash_table_destroy(cache);
  cache_first.next = NULL;
  cache_last.prev = NULL;
  persist_first.next = NULL;
  persist_last.prev = NULL;
}

void
dns_cache_deinit(void)
{
  if (dns_cache_hosts)
    g_free(dns_cache_hosts);
  dns_cache_hosts = NULL;
}
