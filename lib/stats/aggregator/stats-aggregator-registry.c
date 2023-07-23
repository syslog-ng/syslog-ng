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

#include "stats/aggregator/stats-aggregator-registry.h"
#include "stats/stats-registry.h"
#include "stats/stats-cluster.h"
#include "stats/stats-query.h"
#include "cfg.h"
#include "iv.h"
#include "timeutils/cache.h"
#include "mainloop.h"
#include "messages.h"

#define FREQUENCY_OF_UPDATE 60

typedef struct
{
  GHashTable *aggregators;
} StatsAggregatorContainer;

static StatsAggregatorContainer stats_container;

static GMutex stats_aggregator_mutex;
static gboolean stats_aggregator_locked;

void
stats_aggregator_lock(void)
{
  g_mutex_lock(&stats_aggregator_mutex);
  stats_aggregator_locked = TRUE;
}

void
stats_aggregator_unlock(void)
{
  stats_aggregator_locked = FALSE;
  g_mutex_unlock(&stats_aggregator_mutex);
}

/* time based aggregation */



/* handle orphaned of aggregators */

static gboolean
_remove_orphaned_helper(gpointer _key, gpointer _value, gpointer _user_data)
{
  StatsAggregator *aggr = (StatsAggregator *) _value;
  if (stats_aggregator_is_orphaned(aggr))
    {
      stats_aggregator_free(aggr);
      return TRUE;
    }
  return FALSE;
}

void
stats_aggregator_remove_orphaned_stats(void)
{
  g_assert(stats_aggregator_locked);

  g_hash_table_foreach_remove(stats_container.aggregators, _remove_orphaned_helper, NULL);
}

/* reset aggregators, i.e. syslog-ng-ctl stats --reset */

static void
_reset_func (gpointer _key, gpointer _value, gpointer _user_data)
{
  StatsAggregator *aggr = (StatsAggregator *) _value;
  stats_aggregator_reset(aggr);
}

void
stats_aggregator_registry_reset(void)
{
  g_assert(stats_aggregator_locked);
  main_loop_assert_main_thread();

  g_hash_table_foreach(stats_container.aggregators, _reset_func, NULL);
}

/* aggregator cleanup */

static gboolean
_cleanup_aggregator(gpointer _key, gpointer _value, gpointer _user_data)
{
  StatsAggregator *aggr = (StatsAggregator *) _value;
  if (!stats_aggregator_is_orphaned(aggr))
    stats_aggregator_unregister(aggr);

  stats_aggregator_free(aggr);
  return TRUE;
}

static void
stats_aggregator_cleanup(void)
{
  g_assert(stats_aggregator_locked);

  g_hash_table_foreach_remove(stats_container.aggregators, _cleanup_aggregator, NULL);
}

/* module init/deinit */

void
stats_aggregator_registry_init(void)
{
  g_mutex_init(&stats_aggregator_mutex);

  stats_container.aggregators = g_hash_table_new_full((GHashFunc) stats_cluster_key_hash,
                                                      (GEqualFunc) stats_cluster_key_equal, NULL, NULL);
}

void
stats_aggregator_registry_deinit(void)
{
  stats_aggregator_lock();
  stats_aggregator_cleanup();
  stats_aggregator_unlock();
  g_hash_table_destroy(stats_container.aggregators);
  stats_container.aggregators = NULL;
  g_mutex_clear(&stats_aggregator_mutex);
}

/* type specific registration helpers */

static void
_insert_to_table(StatsAggregator *aggr)
{
  g_hash_table_insert(stats_container.aggregators, &aggr->key, aggr);
}

static gboolean
_is_in_table(StatsClusterKey *sc_key)
{
  return g_hash_table_lookup(stats_container.aggregators, sc_key) != NULL;
}

static StatsAggregator *
_get_from_table(StatsClusterKey *sc_key)
{
  return g_hash_table_lookup(stats_container.aggregators, sc_key);
}

void
stats_register_aggregator_maximum(gint level, StatsClusterKey *sc_key, StatsAggregator **aggr)
{
  g_assert(stats_aggregator_locked);

  if (!stats_check_level(level))
    {
      *aggr = NULL;
      return;
    }

  if (!_is_in_table(sc_key))
    {
      *aggr = stats_aggregator_maximum_new(level, sc_key);
      _insert_to_table(*aggr);
    }
  else
    {
      *aggr = _get_from_table(sc_key);
    }

  stats_aggregator_start(*aggr);
}

void
stats_register_aggregator_average(gint level, StatsClusterKey *sc_key, StatsAggregator **aggr)
{
  g_assert(stats_aggregator_locked);

  if (!stats_check_level(level))
    {
      *aggr = NULL;
      return;
    }

  if (!_is_in_table(sc_key))
    {
      *aggr = stats_aggregator_average_new(level, sc_key);
      _insert_to_table(*aggr);
    }
  else
    {
      *aggr = _get_from_table(sc_key);
    }

  stats_aggregator_start(*aggr);
}

void
stats_register_aggregator_cps(gint level, StatsClusterKey *sc_key, StatsClusterKey *sc_key_input, gint stats_type,
                              StatsAggregator **aggr)
{
  g_assert(stats_aggregator_locked);

  if (!stats_check_level(level))
    {
      *aggr = NULL;
      return;
    }

  if (!_is_in_table(sc_key))
    {
      *aggr = stats_aggregator_cps_new(level, sc_key, sc_key_input, stats_type);
      _insert_to_table(*aggr);
    }
  else
    {
      *aggr = _get_from_table(sc_key);
    }

  stats_aggregator_start(*aggr);
}

void
stats_unregister_aggregator(StatsAggregator **aggr)
{
  g_assert(stats_aggregator_locked);
  stats_aggregator_stop(*aggr);
  *aggr = NULL;
}
