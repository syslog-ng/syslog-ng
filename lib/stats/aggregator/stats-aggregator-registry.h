/*
 * Copyright (c) 2021 One Identity
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

#ifndef STATS_AGGREGATOR_REGISTRY_H
#define STATS_AGGREGATOR_REGISTRY_H

#include "stats/aggregator/stats-aggregator.h"
#include "stats/stats-registry.h"
#include "syslog-ng.h"

void stats_aggregator_registry_reset(void);
void stats_aggregator_remove_orphaned_stats(void);

void stats_aggregator_lock(void);
void stats_aggregator_unlock(void);

void stats_aggregator_registry_init(void);
void stats_aggregator_registry_deinit(void);

/* type specific helpers */

void stats_register_aggregator_maximum(gint level, StatsClusterKey *sc_key, StatsAggregator **aggr);
void stats_register_aggregator_average(gint level, StatsClusterKey *sc_key, StatsAggregator **aggr);
void stats_register_aggregator_cps(gint level, StatsClusterKey *sc_key, StatsClusterKey *sc_key_input, gint stats_type,
                                   StatsAggregator **aggr);
void stats_unregister_aggregator(StatsAggregator **aggr);


#endif /* STATS_AGGREGATOR_REGISTRY_H */
