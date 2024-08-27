/*
 * Copyright (c) 2023-2024 Attila Szakacs <attila.szakacs@axoflow.com>
 * Copyright (c) 2024 Balazs Scheidler <balazs.scheidler@axoflow.com>
 * Copyright (c) 2024 László Várady
 * Copyright (c) 2024 Axoflow
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

#include "dyn-metrics-cache.h"
#include "syslog-ng.h"
#include "apphook.h"
#include "tls-support.h"

TLS_BLOCK_START
{
  DynMetricsStore *metrics_cache;
}
TLS_BLOCK_END;

#define metrics_cache __tls_deref(metrics_cache)

static DynMetricsStore *global_metrics_cache;
static GMutex global_metrics_cache_lock;

static void
_sync_with_global_cache(DynMetricsStore *store)
{
  g_mutex_lock(&global_metrics_cache_lock);
  dyn_metrics_store_merge(global_metrics_cache, store);
  g_mutex_unlock(&global_metrics_cache_lock);
}

static void
_purge_global_cache(gint type, gpointer c)
{
  g_mutex_lock(&global_metrics_cache_lock);
  dyn_metrics_store_reset(global_metrics_cache);
  g_mutex_unlock(&global_metrics_cache_lock);
}

static void
_init_tls_cache(gpointer user_data)
{
  g_assert(!metrics_cache);

  metrics_cache = dyn_metrics_store_new();
}

static void
_deinit_tls_cache(gpointer user_data)
{
  _sync_with_global_cache(metrics_cache);
  dyn_metrics_store_free(metrics_cache);
}

DynMetricsStore *
dyn_metrics_cache(void)
{
  return metrics_cache;
}

void
dyn_metrics_cache_global_init(void)
{
  g_assert(!global_metrics_cache);
  g_mutex_init(&global_metrics_cache_lock);
  global_metrics_cache = dyn_metrics_store_new();

  register_application_thread_init_hook(_init_tls_cache, NULL);
  register_application_thread_deinit_hook(_deinit_tls_cache, NULL);

  register_application_hook(AH_CONFIG_CHANGED, _purge_global_cache, NULL, AHM_RUN_REPEAT);

  _init_tls_cache(NULL);
}

void
dyn_metrics_cache_global_deinit(void)
{
  _deinit_tls_cache(NULL);

  dyn_metrics_store_free(global_metrics_cache);
  g_mutex_clear(&global_metrics_cache_lock);
}
