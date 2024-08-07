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

#ifndef METRICS_CACHE_H_INCLUDED
#define METRICS_CACHE_H_INCLUDED

#include "stats/stats-registry.h"

/*
 * There is a recurring inconvenience with dynamic counters.
 *
 * Registering, changing and unregistering a counter makes it orphaned,
 * as no one is keeping it alive. Non-dynamic counters do not have this
 * issue, as they are always stored on their call-site, binding their
 * lifecycle to the call-site.
 *
 * This class intends to solve this problem by providing a cache,
 * which keeps alive its counters until the cache is freed. On the
 * call-site you only need to keep the cache alive, to keep the
 * counters alive.
 *
 * It also grants a label cache for performance optimization needs.
 *
 * Note: The cache is NOT thread safe, make sure to eliminate
 * concurrency on the call site. If you need a cache that is bound
 * to the current thread, see metrics/metrics-tls-cache.h.
 */

typedef struct _MetricsCache MetricsCache;

MetricsCache *metrics_cache_new(void);
void metrics_cache_free(MetricsCache *self);

StatsCounterItem *metrics_cache_get_counter(MetricsCache *self, StatsClusterKey *key, gint level);
gboolean metrics_cache_remove_counter(MetricsCache *self, StatsClusterKey *key);
void metrics_cache_reset_labels(MetricsCache *self);
StatsClusterLabel *metrics_cache_alloc_label(MetricsCache *self);
StatsClusterLabel *metrics_cache_get_labels(MetricsCache *self);
guint metrics_cache_get_labels_len(MetricsCache *self);

#endif
