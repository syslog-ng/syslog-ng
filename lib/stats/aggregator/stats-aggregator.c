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

void
stats_aggregator_track_counter(StatsAggregator *self)
{
  if (!self)
    return;

  if (stats_aggregator_is_orphaned(self))
    stats_aggregator_register(self);

  ++self->use_count;
}

void
stats_aggregator_untrack_counter(StatsAggregator *self)
{
  if (!self)
    return;

  if (self->use_count > 0)
    --self->use_count;

  if (self->use_count == 0)
    stats_aggregator_aggregate(self);

  if (stats_aggregator_is_orphaned(self))
    stats_aggregator_unregister(self);
}

static gboolean
_is_orphaned(StatsAggregator *self)
{
  return self->use_count == 0;
}

static void
_set_virtual_functions(StatsAggregator *self)
{
  self->is_orphaned = _is_orphaned;
}

void
stats_aggregator_init_instance(StatsAggregator *self, StatsClusterKey *sc_key, gint stats_level)
{
  self->use_count = 0;
  self->stats_level = stats_level;
  stats_cluster_key_clone(&self->key, sc_key);
  _set_virtual_functions(self);
}

void
stats_aggregator_free(StatsAggregator *self)
{
  stats_cluster_key_cloned_free(&self->key);
  if (self && self->free_fn)
    self->free_fn(self);
  g_free(self);
}
