/*
 * Copyright (c) 2007-2016 Balabit
 * Copyright (c) 2007-2013 Balázs Scheidler
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

#include <criterion/criterion.h>

#include "dnscache.h"
#include "apphook.h"
#include "timeutils/cache.h"
#include "timeutils/misc.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

static const char *positive_hostname = "hostname";
static const char *negative_hostname = "negative";

static void
_fill_dns_cache(DNSCache *cache, gint cache_size)
{
  gint i;
  for (i = 0; i < cache_size; i++)
    {
      guint32 ni = htonl(i);
      gboolean positive = i < (cache_size / 2);
      dns_cache_store_dynamic(cache, AF_INET, (void *) &ni,
                              positive ? positive_hostname : negative_hostname, positive);
    }
}

static void
_fill_benchmark_dns_cache(DNSCache *cache, gint cache_size)
{
  gint i;
  for (i = 0; i < cache_size; i++)
    {
      guint32 ni = htonl(i);
      dns_cache_store_dynamic(cache, AF_INET, (void *) &ni, positive_hostname, TRUE);
    }
}

static void
_invalidate_with_sleep(guint seconds)
{
  sleep(seconds);
  invalidate_cached_time();
}

void
assert_no_forget(DNSCache *cache, gint cache_size)
{
  gint i;
  const gchar *hn = NULL;
  gsize hn_len;
  gboolean positive;
  gint positive_limit = cache_size / 2;

  for (i = 0; i < cache_size; i++)
    {
      guint32 ni = htonl(i);

      hn = NULL;
      positive = FALSE;

      cr_assert(dns_cache_lookup(cache, AF_INET, (void *) &ni, &hn, &hn_len, &positive),
                "hmmm cache forgot the cache entry too early, i=%d, hn=%s\n",
                i, hn);

      if (i < positive_limit)
        {
          cr_assert(positive && strcmp(hn, "hostname") == 0,
                    "hmm, cached returned an positive match, but cached name invalid, i=%d, hn=%s\n",
                    i, hn);
        }
      else
        {
          cr_assert(!positive && strcmp(hn, "negative") == 0,
                    "hmm, cache returned a positive match, where a negative match was expected, i=%d, hn=%s\n",
                    i, hn);
        }

    }
}

void
assert_forget_negative(DNSCache *cache, gint cache_size)
{
  gint i;
  const gchar *hn = NULL;
  gsize hn_len;
  gboolean positive;
  gint positive_limit = cache_size / 2;

  for (i = 0; i < cache_size; i++)
    {
      guint32 ni = htonl(i);

      hn = NULL;
      positive = FALSE;
      if (i < positive_limit)
        {
          cr_assert(dns_cache_lookup(cache, AF_INET, (void *) &ni, &hn, &hn_len, &positive) || !positive,
                    "hmmm cache forgot positive entries too early, i=%d\n",
                    i);
        }
      else
        {
          cr_assert_not(dns_cache_lookup(cache, AF_INET, (void *) &ni, &hn, &hn_len, &positive) || positive,
                        "hmmm cache didn't forget negative entries in time, i=%d\n",
                        i);
        }
    }
}

void
assert_forget_all(DNSCache *cache, gint cache_size)
{
  gint i;
  const gchar *hn = NULL;
  gsize hn_len;
  gboolean positive;

  for (i = 0; i < cache_size; i++)
    {
      guint32 ni = htonl(i);

      hn = NULL;
      positive = FALSE;
      cr_assert_not(dns_cache_lookup(cache, AF_INET, (void *) &ni, &hn, &hn_len, &positive),
                    "hmmm cache did not forget an expired entry, i=%d\n",
                    i);
    }
}


TestSuite(dnscache, .init = app_startup, .fini = app_shutdown);

Test(dnscache, test_expiration)
{
  DNSCacheOptions options =
  {
    .cache_size = 50000,
    .expire = 3,
    .expire_failed = 1,
    .hosts = NULL
  };

  DNSCache *cache = dns_cache_new(&options);
  gint cache_size = 10000;
  _fill_dns_cache(cache, cache_size);

  assert_no_forget(cache, cache_size);

  /* negative entries should expire by now, positive ones still present */
  _invalidate_with_sleep(2);
  assert_forget_negative(cache, cache_size);

  /* everything should be expired by now */
  _invalidate_with_sleep(2);
  assert_forget_all(cache, cache_size);

  dns_cache_free(cache);
}

Test(dnscache, test_run_benchmark)
{
  DNSCacheOptions options =
  {
    .cache_size = 50000,
    .expire = 600,
    .expire_failed = 300,
    .hosts = NULL
  };

  DNSCache *cache = dns_cache_new(&options);
  GTimeVal start, end;
  const gchar *hn;
  gsize hn_len;
  gboolean positive;
  gint i;

  gint cache_size = 10000;
  _fill_benchmark_dns_cache(cache, cache_size);

  g_get_current_time(&start);
  /* run benchmarks */
  for (i = 0; i < cache_size; i++)
    {
      guint32 ni = htonl(i % cache_size);

      hn = NULL;
      dns_cache_lookup(cache, AF_INET, (void *) &ni, &hn, &hn_len, &positive);
    }
  g_get_current_time(&end);
  printf("DNS cache speed: %12.3f iters/sec\n", i * 1e6 / g_time_val_diff(&end, &start));

  dns_cache_free(cache);
}

/* test case to check the LRU expiration functionality */
Test(dnscache, test_lru_lists)
{
  DNSCacheOptions options =
  {
    .cache_size = 10,
    .expire = 600,
    .expire_failed = 300,
    .hosts = NULL
  };


  DNSCache *cache = dns_cache_new(&options);
  gint cache_size = 100;
  _fill_dns_cache(cache, cache_size);
  dns_cache_free(cache);
}
