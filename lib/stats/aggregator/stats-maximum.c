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
} StatsAggregatorMaximum;

static void
_add_data_point(StatsAggregator *s, gsize value)
{
  StatsAggregatorMaximum *self = (StatsAggregatorMaximum *)s;
  gsize current_max = 0;

  do
    {
      current_max = stats_counter_get(self->super.output_counter);

      if (current_max >= value)
        break;

    }
  while(!atomic_gssize_compare_and_exchange(&self->super.output_counter->value, current_max, value));
}

static void
_set_virtual_function(StatsAggregatorMaximum *self)
{
  self->super.add_data_point = _add_data_point;
}

StatsAggregator *
stats_aggregator_maximum_new(gint level, StatsClusterKey *sc_key)
{
  StatsAggregatorMaximum *self = g_new0(StatsAggregatorMaximum, 1);
  stats_aggregator_init_instance(&self->super, sc_key, level);
  _set_virtual_function(self);

  return &self->super;
}
