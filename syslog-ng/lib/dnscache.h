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


#ifndef DNSCACHE_H_INCLUDED
#define DNSCACHE_H_INCLUDED

#include "syslog-ng.h"

typedef struct
{
  gint cache_size;
  gint expire;
  gint expire_failed;
  gchar *hosts;
} DNSCacheOptions;

typedef struct _DNSCache DNSCache;

void dns_cache_store_persistent(DNSCache *self, gint family, void *addr, const gchar *hostname);
void dns_cache_store_dynamic(DNSCache *self, gint family, void *addr, const gchar *hostname, gboolean positive);
gboolean dns_cache_lookup(DNSCache *self, gint family, void *addr, const gchar **hostname, gsize *hostname_len,
                          gboolean *positive);
DNSCache *dns_cache_new(const DNSCacheOptions *options);
void dns_cache_free(DNSCache *self);

void dns_cache_options_defaults(DNSCacheOptions *options);
void dns_cache_options_destroy(DNSCacheOptions *options);

gboolean dns_caching_lookup(gint family, void *addr, const gchar **hostname, gsize *hostname_len, gboolean *positive);
void dns_caching_store(gint family, void *addr, const gchar *hostname, gboolean positive);
void dns_caching_update_options(const DNSCacheOptions *dns_cache_options);

void dns_caching_thread_init(void);
void dns_caching_thread_deinit(void);
void dns_caching_global_init(void);
void dns_caching_global_deinit(void);


#endif
