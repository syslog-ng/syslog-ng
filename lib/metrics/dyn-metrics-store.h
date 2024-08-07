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

#ifndef DYN_METRICS_STORE_H_INCLUDED
#define DYN_METRICS_STORE_H_INCLUDED

#include "stats/stats-registry.h"

/*
 * There is a recurring inconvenience with dynamic counters.
 *
 * Registering, changing and unregistering a counter makes it orphaned,
 * as no one is keeping it alive. Non-dynamic counters do not have this
 * issue, as they are always stored on their call-site, binding their
 * lifecycle to the call-site.
 *
 * This class intends to solve this problem by providing a store,
 * which keeps alive its counters until the store is freed. On the
 * call-site you only need to keep the store alive, to keep the
 * counters alive.
 *
 * It also grants a label cache for performance optimization needs.
 *
 * Note: The store is NOT thread safe, make sure to eliminate
 * concurrency on the call site. If you need a cache that is purged only
 * during reload, use metrics/dyn-metrics-cache.h.
 *
 */

typedef struct _DynMetricsStore DynMetricsStore;

DynMetricsStore *dyn_metrics_store_new(void);
void dyn_metrics_store_free(DynMetricsStore *self);

StatsCounterItem *dyn_metrics_store_retrieve_counter(DynMetricsStore *self, StatsClusterKey *key, gint level);
gboolean dyn_metrics_store_remove_counter(DynMetricsStore *self, StatsClusterKey *key);
void dyn_metrics_store_reset(DynMetricsStore *self);
void dyn_metrics_store_merge(DynMetricsStore *self, DynMetricsStore *other);

void dyn_metrics_store_reset_labels_cache(DynMetricsStore *self);
StatsClusterLabel *dyn_metrics_store_cache_label(DynMetricsStore *self);
StatsClusterLabel *dyn_metrics_store_get_cached_labels(DynMetricsStore *self);
guint dyn_metrics_store_get_cached_labels_len(DynMetricsStore *self);
void dyn_metrics_store_sort_cached_labels(DynMetricsStore *self);

#endif
