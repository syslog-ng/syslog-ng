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

#ifndef STATS_AGGREGATOR_H
#define STATS_AGGREGATOR_H

#include "stats/stats-counter.h"
#include "stats/stats-cluster.h"
#include <iv.h>

typedef struct _StatsAggregator StatsAggregator;

struct _StatsAggregator
{
  void (*add_data_point)(StatsAggregator *self, gsize value);
  void (*aggregate)(StatsAggregator *self);
  void (*reset)(StatsAggregator *self);
  void (*free_fn)(StatsAggregator *self);
  gboolean (*is_orphaned)(StatsAggregator *self);

  void (*register_aggr)(StatsAggregator *self);
  void (*unregister_aggr)(StatsAggregator *self);

  gssize use_count;
  StatsClusterKey key;
  StatsCounterItem *output_counter;
  gint stats_level;
  gint timer_period;
  struct iv_timer update_timer;
};

static inline void
stats_aggregator_add_data_point(StatsAggregator *self, gsize value)
{
  if (self && self->add_data_point)
    self->add_data_point(self, value);
}

static inline void
stats_aggregator_aggregate(StatsAggregator *self)
{
  if (self && self->aggregate)
    self->aggregate(self);
}

static inline void
stats_aggregator_reset(StatsAggregator *self)
{
  if (self && self->reset)
    self->reset(self);
}

static inline gboolean
stats_aggregator_is_orphaned(StatsAggregator *self)
{
  if (self && self->is_orphaned)
    return self->is_orphaned(self);

  return TRUE;
}

void stats_aggregator_register(StatsAggregator *self);
void stats_aggregator_unregister(StatsAggregator *self);

/* public */
void stats_aggregator_add_data_point(StatsAggregator *self, gsize value);
void stats_aggregator_aggregate(StatsAggregator *self);
void stats_aggregator_reset(StatsAggregator *self);

/* stats-internals */
void stats_aggregator_start(StatsAggregator *self);
void stats_aggregator_stop(StatsAggregator *self);
void stats_aggregator_init_instance(StatsAggregator *self, StatsClusterKey *sc_key, gint stats_level);
void stats_aggregator_free(StatsAggregator *self);

/* type specific constructors */

StatsAggregator *stats_aggregator_maximum_new(gint level, StatsClusterKey *sc_key);

StatsAggregator *stats_aggregator_average_new(gint level, StatsClusterKey *sc_key);

StatsAggregator *stats_aggregator_cps_new(gint level, StatsClusterKey *sc_key, StatsClusterKey *sc_key_input,
                                          gint stats_type);

#endif /* STATS_AGGREGATOR_H */
