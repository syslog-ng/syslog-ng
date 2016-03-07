/*
 * Copyright (c) 2007-2013 Balabit
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

#include "dnscache.h"
#include "apphook.h"
#include "timeutils.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

void
test_expiration(void)
{
  gint i;
  const gchar *hn = NULL;
  gsize hn_len;
  gboolean positive;
  DNSCacheOptions options =
  {
    .cache_size = 50000,
    .expire = 3,
    .expire_failed = 1,
    .hosts = NULL
  };
  DNSCache *cache = dns_cache_new(&options);

  for (i = 0; i < 10000; i++)
    {
      guint32 ni = htonl(i);

      dns_cache_store_dynamic(cache, AF_INET, (void *) &ni, i < 5000 ? "hostname" : "negative", i < 5000);
    }

  for (i = 0; i < 10000; i++)
    {
      guint32 ni = htonl(i);

      hn = NULL;
      positive = FALSE;
      if (!dns_cache_lookup(cache, AF_INET, (void *) &ni, &hn, &hn_len, &positive))
        {
          fprintf(stderr, "hmmm cache forgot the cache entry too early, i=%d, hn=%s\n", i, hn);
          exit(1);
        }
      else
        {
          if (i < 5000)
            {
              if (!positive || strcmp(hn, "hostname") != 0)
                {
                  fprintf(stderr, "hmm, cached returned an positive match, but cached name invalid, i=%d, hn=%s\n", i, hn);
                  exit(1);
                }
            }
          else
            {
              if (positive || strcmp(hn, "negative") != 0)
                {
                  fprintf(stderr, "hmm, cache returned a positive match, where a negative match was expected, i=%d, hn=%s\n", i, hn);
                  exit(1);
                }
            }

        }
    }

  /* negative entries should expire by now, positive ones still present */
  sleep(2);
  invalidate_cached_time();

  for (i = 0; i < 10000; i++)
    {
      guint32 ni = htonl(i);

      hn = NULL;
      positive = FALSE;
      if (i < 5000)
        {
          if (!dns_cache_lookup(cache, AF_INET, (void *) &ni, &hn, &hn_len, &positive) || !positive)
            {
              fprintf(stderr, "hmmm cache forgot positive entries too early, i=%d\n", i);
              exit(1);
            }
        }
      else
        {
          if (dns_cache_lookup(cache, AF_INET, (void *) &ni, &hn, &hn_len, &positive) || positive)
            {
              fprintf(stderr, "hmmm cache didn't forget negative entries in time, i=%d\n", i);
              exit(1);
            }
        }
    }

  /* everything should be expired by now */

  sleep(2);
  invalidate_cached_time();

  for (i = 0; i < 10000; i++)
    {
      guint32 ni = htonl(i);

      hn = NULL;
      positive = FALSE;
      if (dns_cache_lookup(cache, AF_INET, (void *) &ni, &hn, &hn_len, &positive))
        {
          fprintf(stderr, "hmmm cache did not forget an expired entry, i=%d\n", i);
          exit(1);
        }
    }

  dns_cache_free(cache);
}

void
test_dns_cache_benchmark(void)
{
  GTimeVal start, end;
  const gchar *hn;
  gsize hn_len;
  gboolean positive;
  gint i;
  DNSCacheOptions options =
  {
    .cache_size = 50000,
    .expire = 600,
    .expire_failed = 300,
    .hosts = NULL
  };
  DNSCache *cache = dns_cache_new(&options);

  for (i = 0; i < 10000; i++)
    {
      guint32 ni = htonl(i);

      dns_cache_store_dynamic(cache, AF_INET, (void *) &ni, "hostname", TRUE);
    }

  g_get_current_time(&start);
  /* run benchmarks */
  for (i = 0; i < 10000; i++)
    {
      guint32 ni = htonl(i % 10000);

      hn = NULL;
      if (!dns_cache_lookup(cache, AF_INET, (void *) &ni, &hn, &hn_len, &positive))
        {
          fprintf(stderr, "hmm, dns cache entries expired during benchmarking, this is unexpected\n, i=%d", i);
        }
    }
  g_get_current_time(&end);
  printf("DNS cache speed: %12.3f iters/sec\n", i * 1e6 / g_time_val_diff(&end, &start));
  dns_cache_free(cache);
}

int
main()
{
  app_startup();

  test_expiration();
  test_dns_cache_benchmark();

  app_shutdown();
  return 0;
}
