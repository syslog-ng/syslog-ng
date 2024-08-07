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

#include "dyn-metrics-store.h"
#include "stats/stats-cluster-single.h"

#include <string.h>

struct _DynMetricsStore
{
  GHashTable *clusters;
  GArray *label_buffers;
};

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

DynMetricsStore *
dyn_metrics_store_new(void)
{
  DynMetricsStore *self = g_new0(DynMetricsStore, 1);

  self->clusters = g_hash_table_new_full((GHashFunc) stats_cluster_key_hash,
                                         (GEqualFunc) stats_cluster_key_equal,
                                         NULL,
                                         (GDestroyNotify) _unregister_single_cluster_locked);
  self->label_buffers = g_array_new(FALSE, FALSE, sizeof(StatsClusterLabel));

  return self;
}

void
dyn_metrics_store_free(DynMetricsStore *self)
{
  g_hash_table_destroy(self->clusters);
  g_array_free(self->label_buffers, TRUE);
  g_free(self);
}

StatsCounterItem *
dyn_metrics_store_get_counter(DynMetricsStore *self, StatsClusterKey *key, gint level)
{
  StatsCluster *cluster = g_hash_table_lookup(self->clusters, key);
  if (!cluster)
    {
      cluster = _register_single_cluster_locked(key, level);
      if (cluster)
        g_hash_table_insert(self->clusters, &cluster->key, cluster);
    }

  return stats_cluster_single_get_counter(cluster);
}

gboolean
dyn_metrics_store_remove_counter(DynMetricsStore *self, StatsClusterKey *key)
{
  return g_hash_table_remove(self->clusters, key);
}

void
dyn_metrics_store_reset_labels(DynMetricsStore *self)
{
  self->label_buffers = g_array_set_size(self->label_buffers, 0);
}

StatsClusterLabel *
dyn_metrics_store_alloc_label(DynMetricsStore *self)
{
  self->label_buffers = g_array_set_size(self->label_buffers, self->label_buffers->len + 1);
  return &g_array_index(self->label_buffers, StatsClusterLabel, self->label_buffers->len - 1);
}

StatsClusterLabel *
dyn_metrics_store_get_labels(DynMetricsStore *self)
{
  return (StatsClusterLabel *) self->label_buffers->data;
}

guint
dyn_metrics_store_get_labels_len(DynMetricsStore *self)
{
  return self->label_buffers->len;
}

static gint
_label_cmp(StatsClusterLabel *lhs, StatsClusterLabel *rhs)
{
  return strcmp(lhs->name, rhs->name);
}

void
dyn_metrics_store_sort_labels(DynMetricsStore *self)
{
  g_array_sort(self->label_buffers, (GCompareFunc) _label_cmp);
}
