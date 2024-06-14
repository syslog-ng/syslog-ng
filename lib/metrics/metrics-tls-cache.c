/*
 * Copyright (c) 2023-2024 Attila Szakacs <attila.szakacs@axoflow.com>
 * Copyright (c) 2024 Balazs Scheidler <balazs.scheidler@axoflow.com>
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

#include "metrics-tls-cache.h"
#include "stats/stats-cluster-single.h"
#include "apphook.h"
#include "tls-support.h"

TLS_BLOCK_START
{
  GHashTable *clusters;
  GArray *label_buffers;
}
TLS_BLOCK_END;

#define clusters __tls_deref(clusters)
#define label_buffers __tls_deref(label_buffers)

static StatsCluster *
_register_single_cluster_locked(StatsClusterKey *key, gint stats_level)
{
  StatsCluster *cluster;

  stats_lock();
  {
    StatsCounterItem *counter;
    cluster = stats_register_dynamic_counter(stats_level, key, SC_TYPE_SINGLE_VALUE, &counter);
  }
  stats_unlock();

  return cluster;
}

static void
_unregister_single_cluster_locked(StatsCluster *cluster)
{
  stats_lock();
  {
    StatsCounterItem *counter = stats_cluster_single_get_counter(cluster);
    stats_unregister_dynamic_counter(cluster, SC_TYPE_SINGLE_VALUE, &counter);
  }
  stats_unlock();
}

static void
_init_tls_clusters_map_thread_init_hook(gpointer user_data)
{
  g_assert(!clusters && !label_buffers);

  clusters = g_hash_table_new_full((GHashFunc) stats_cluster_key_hash,
                                   (GEqualFunc) stats_cluster_key_equal,
                                   NULL,
                                   (GDestroyNotify) _unregister_single_cluster_locked);
  label_buffers = g_array_new(FALSE, FALSE, sizeof(StatsClusterLabel));
}

static void
_deinit_tls_clusters_map_thread_init_hook(gpointer user_data)
{
  g_hash_table_destroy(clusters);
  g_array_free(label_buffers, TRUE);
}

static void
_init_tls_clusters_map_apphook(gint type, gpointer user_data)
{
  _init_tls_clusters_map_thread_init_hook(user_data);
}

static void
_deinit_tls_clusters_map_apphook(gint type, gpointer user_data)
{
  _deinit_tls_clusters_map_thread_init_hook(user_data);
}


StatsCounterItem *
metrics_tls_cache_get_counter(StatsClusterKey *key, gint level)
{
  StatsCluster *cluster = g_hash_table_lookup(clusters, key);
  if (!cluster)
    {
      cluster = _register_single_cluster_locked(key, level);
      if (cluster)
        g_hash_table_insert(clusters, &cluster->key, cluster);
    }

  return stats_cluster_single_get_counter(cluster);
}

void
metrics_tls_cache_reset_labels(void)
{
  label_buffers = g_array_set_size(label_buffers, 0);
}

void
metrics_tls_cache_append_label(StatsClusterLabel *label)
{
  label_buffers = g_array_append_vals(label_buffers, label, 1);
}

StatsClusterLabel *
metrics_tls_cache_get_next_label(void)
{
  label_buffers = g_array_set_size(label_buffers, label_buffers->len + 1);
  return &g_array_index(label_buffers, StatsClusterLabel, label_buffers->len - 1);
}

StatsClusterLabel *
metrics_tls_cache_get_labels(void)
{
  return (StatsClusterLabel *) label_buffers->data;
}

guint
metrics_tls_cache_get_labels_len(void)
{
  return label_buffers->len;
}

void
metrics_tls_cache_global_init(void)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      register_application_thread_init_hook(_init_tls_clusters_map_thread_init_hook, NULL);
      register_application_thread_deinit_hook(_deinit_tls_clusters_map_thread_init_hook, NULL);
      register_application_hook(AH_STARTUP, _init_tls_clusters_map_apphook, NULL, AHM_RUN_ONCE);
      register_application_hook(AH_SHUTDOWN, _deinit_tls_clusters_map_apphook, NULL, AHM_RUN_ONCE);
      initialized = TRUE;
    }
}
