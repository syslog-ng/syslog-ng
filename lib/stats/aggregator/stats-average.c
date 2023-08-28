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

#include "stats/aggregator/stats-aggregator.h"
#include "stats/stats-registry.h"
#include "stats/stats-cluster-single.h"

typedef struct
{
  StatsAggregator super;
  atomic_gssize count;
  atomic_gssize sum;
} StatsAggregatorAverage;

static inline void
_inc_count(StatsAggregatorAverage *self)
{
  atomic_gssize_inc(&self->count);
}

static inline void
_add_sum(StatsAggregatorAverage *self, gsize value)
{
  atomic_gssize_add(&self->sum, value);
}

static inline gsize
_get_sum(StatsAggregatorAverage *self)
{
  return atomic_gssize_get_unsigned(&self->sum);
}

static inline gsize
_get_count(StatsAggregatorAverage *self)
{
  return atomic_gssize_get_unsigned(&self->count);
}

static void
_add_data_point(StatsAggregator *s, gsize value)
{
  StatsAggregatorAverage *self = (StatsAggregatorAverage *)s;
  /*
   * this function isn't truely atomic, reset might set count to zero
   * and we want to avoid zero division
   */
  gsize sum = _get_sum(self) + value;
  gsize divisor = _get_count(self) + 1;

  stats_counter_set(self->super.output_counter, sum/divisor);

  _add_sum(self, value);
  _inc_count(self);
}

static void
_reset(StatsAggregator *s)
{
  StatsAggregatorAverage *self = (StatsAggregatorAverage *)s;
  atomic_gssize_set(&self->count, 0);
  atomic_gssize_set(&self->sum, 0);
  stats_counter_set(self->super.output_counter, 0);
}

static void
_set_virtual_function(StatsAggregatorAverage *self )
{
  self->super.add_data_point = _add_data_point;
  self->super.reset = _reset;
}

StatsAggregator *
stats_aggregator_average_new(gint level, StatsClusterKey *sc_key)
{
  StatsAggregatorAverage *self = g_new0(StatsAggregatorAverage, 1);
  stats_aggregator_init_instance(&self->super, sc_key, level);
  _set_virtual_function(self);

  return &self->super;
}
