/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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
#include "stats/stats-registry.h"
#include "stats/stats-query.h"
#include "cfg.h"
#include <string.h>

typedef struct _StatsClusterContainer
{
  GHashTable *static_clusters;
  GHashTable *dynamic_clusters;
} StatsClusterContainer;

static StatsClusterContainer stats_cluster_container;

static GStaticMutex stats_mutex = G_STATIC_MUTEX_INIT;
gboolean stats_locked;

static void
_insert_cluster(StatsCluster *sc)
{
  if (sc->dynamic)
    g_hash_table_insert(stats_cluster_container.dynamic_clusters, &sc->key, sc);
  else
    g_hash_table_insert(stats_cluster_container.static_clusters, &sc->key, sc);
}

void
stats_lock(void)
{
  g_static_mutex_lock(&stats_mutex);
  stats_locked = TRUE;
}

void
stats_unlock(void)
{
  stats_locked = FALSE;
  g_static_mutex_unlock(&stats_mutex);
}

static StatsCluster *
_grab_dynamic_cluster(const StatsClusterKey *sc_key)
{
  StatsCluster *sc;

  sc = g_hash_table_lookup(stats_cluster_container.dynamic_clusters, sc_key);
  if (!sc)
    {
      sc = stats_cluster_dynamic_new(sc_key);
      _insert_cluster(sc);
    }

  return sc;

}

static StatsCluster *
_grab_static_cluster(const StatsClusterKey *sc_key)
{
  StatsCluster *sc;

  sc = g_hash_table_lookup(stats_cluster_container.static_clusters, sc_key);
  if (!sc)
    {
      sc = stats_cluster_new(sc_key);
      _insert_cluster(sc);
    }

  return sc;
}

static StatsCluster *
_grab_cluster(gint stats_level, const StatsClusterKey *sc_key, gboolean dynamic)
{
  if (!stats_check_level(stats_level))
    return NULL;

  StatsCluster *sc = NULL;

  if (dynamic)
    sc = _grab_dynamic_cluster(sc_key);
  else
    sc = _grab_static_cluster(sc_key);

  if (!sc)
    return NULL;

  /* check that we are not overwriting a dynamic counter with a
   * non-dynamic one or vica versa.  This could only happen if the same
   * key is used for both a dynamic counter and a non-dynamic one, which
   * is a programming error */

  g_assert(sc->dynamic == dynamic);
  return sc;
}

static StatsCluster *
_register_counter(gint stats_level, const StatsClusterKey *sc_key, gint type,
                  gboolean dynamic, StatsCounterItem **counter)
{
  StatsCluster *sc;

  g_assert(stats_locked);

  sc = _grab_cluster(stats_level, sc_key, dynamic);
  if (sc)
    {
      *counter = stats_cluster_track_counter(sc, type);
      (*counter)->type = type;
    }
  else
    {
      *counter = NULL;
    }
  return sc;
}

/**
 * stats_register_counter:
 * @stats_level: the required statistics level to make this counter available
 * @component: a reference to the syslog-ng component that this counter belongs to (SCS_*)
 * @id: the unique identifier of the configuration item that this counter belongs to
 * @instance: if a given configuration item manages multiple similar counters
 *            this makes those unique (like destination filename in case macros are used)
 * @type: the counter type (processed, dropped, etc)
 * @counter: returned pointer to the counter
 *
 * This fuction registers a general purpose counter. Whenever multiple
 * objects touch the same counter all of these should register the counter
 * with the same name. Internally the stats subsystem counts the number of
 * users of the same counter in this case, thus the counter will only be
 * freed when all of these uses are unregistered.
 **/
StatsCluster *
stats_register_counter(gint stats_level, const StatsClusterKey *sc_key, gint type,
                       StatsCounterItem **counter)
{
  return _register_counter(stats_level, sc_key, type, FALSE, counter);
}

StatsCluster *
stats_register_counter_and_index(gint stats_level, const StatsClusterKey *sc_key, gint type,
                                 StatsCounterItem **counter)
{
  StatsCluster *cluster =  _register_counter(stats_level, sc_key, type, FALSE, counter);
  if (cluster)
    stats_query_index_counter(cluster, type);

  return cluster;
}

StatsCluster *
stats_register_dynamic_counter(gint stats_level, const StatsClusterKey *sc_key,
                               gint type, StatsCounterItem **counter)
{
  return _register_counter(stats_level, sc_key, type, TRUE, counter);
}

/*
 * stats_instant_inc_dynamic_counter
 * @timestamp: if non-negative, an associated timestamp will be created and set
 *
 * Instantly create (if not exists) and increment a dynamic counter.
 */
void
stats_register_and_increment_dynamic_counter(gint stats_level, const StatsClusterKey *sc_key,
                                             time_t timestamp)
{
  StatsCounterItem *counter, *stamp;
  StatsCluster *handle;

  g_assert(stats_locked);
  handle = stats_register_dynamic_counter(stats_level, sc_key, SC_TYPE_PROCESSED, &counter);
  if (!handle)
    return;
  stats_counter_inc(counter);
  if (timestamp >= 0)
    {
      stats_register_associated_counter(handle, SC_TYPE_STAMP, &stamp);
      stats_counter_set(stamp, timestamp);
      stats_unregister_dynamic_counter(handle, SC_TYPE_STAMP, &stamp);
    }
  stats_unregister_dynamic_counter(handle, SC_TYPE_PROCESSED, &counter);
}

/**
 * stats_register_associated_counter:
 * @sc: the dynamic counter that was registered with stats_register_dynamic_counter
 * @type: the type that we want to use in the same StatsCluster instance
 * @counter: the returned pointer to the counter itself
 *
 * This function registers another counter type in the same StatsCounter
 * instance in order to avoid an unnecessary lookup.
 **/
void
stats_register_associated_counter(StatsCluster *sc, gint type, StatsCounterItem **counter)
{
  g_assert(stats_locked);

  *counter = NULL;
  if (!sc)
    return;
  g_assert(sc->dynamic);

  *counter = stats_cluster_track_counter(sc, type);
}

void
stats_unregister_counter(const StatsClusterKey *sc_key, gint type,
                         StatsCounterItem **counter)
{
  StatsCluster *sc;

  g_assert(stats_locked);

  if (*counter == NULL)
    return;

  sc = g_hash_table_lookup(stats_cluster_container.static_clusters, sc_key);

  stats_cluster_untrack_counter(sc, type, counter);
}

void
stats_unregister_dynamic_counter(StatsCluster *sc, gint type, StatsCounterItem **counter)
{
  g_assert(stats_locked);
  if (!sc)
    return;
  stats_cluster_untrack_counter(sc, type, counter);
}

void
save_counter_to_persistent_storage(GlobalConfig *cfg, StatsCounterItem *counter)
{
  if (counter)
    {
      g_assert(counter->name);
      gssize *orig_value = g_new(gssize, 1);
      *orig_value = stats_counter_get(counter);
      cfg_persist_config_add(cfg, counter->name, orig_value, (GDestroyNotify) g_free, FALSE);
    }
}

void
load_counter_from_persistent_storage(GlobalConfig *cfg, StatsCounterItem *counter)
{
  g_assert(counter->name);
  gssize *orig_value = cfg_persist_config_fetch(cfg, counter->name);
  if (orig_value)
    stats_counter_set(counter, *orig_value);
}

static void
_foreach_cluster_helper(gpointer key, gpointer value, gpointer user_data)
{
  gpointer *args = (gpointer *) user_data;
  StatsForeachClusterFunc func = args[0];
  gpointer func_data = args[1];
  StatsCluster *sc = (StatsCluster *) value;

  func(sc, func_data);
}

void
stats_foreach_cluster(StatsForeachClusterFunc func, gpointer user_data)
{
  gpointer args[] = { func, user_data };

  g_assert(stats_locked);
  g_hash_table_foreach(stats_cluster_container.static_clusters, _foreach_cluster_helper, args);
  g_hash_table_foreach(stats_cluster_container.dynamic_clusters, _foreach_cluster_helper, args);
}

static gboolean
_foreach_cluster_remove_helper(gpointer key, gpointer value, gpointer user_data)
{
  gpointer *args = (gpointer *) user_data;
  StatsForeachClusterRemoveFunc func = args[0];
  gpointer func_data = args[1];
  StatsCluster *sc = (StatsCluster *) value;

  return func(sc, func_data);
}

void
stats_foreach_cluster_remove(StatsForeachClusterRemoveFunc func, gpointer user_data)
{
  gpointer args[] = { func, user_data };
  g_hash_table_foreach_remove(stats_cluster_container.static_clusters, _foreach_cluster_remove_helper, args);
  g_hash_table_foreach_remove(stats_cluster_container.dynamic_clusters, _foreach_cluster_remove_helper, args);
}

static void
_foreach_counter_helper(StatsCluster *sc, gpointer user_data)
{
  gpointer *args = (gpointer *) user_data;
  StatsForeachCounterFunc func = args[0];
  gpointer func_data = args[1];

  stats_cluster_foreach_counter(sc, func, func_data);
}

void
stats_foreach_counter(StatsForeachCounterFunc func, gpointer user_data)
{
  gpointer args[] = { func, user_data };

  g_assert(stats_locked);
  stats_foreach_cluster(_foreach_counter_helper, args);
}

void
stats_registry_init(void)
{
  stats_cluster_container.static_clusters = g_hash_table_new_full((GHashFunc) stats_cluster_hash,
                                            (GEqualFunc) stats_cluster_equal, NULL,
                                            (GDestroyNotify) stats_cluster_free);
  stats_cluster_container.dynamic_clusters = g_hash_table_new_full((GHashFunc) stats_cluster_hash,
                                             (GEqualFunc) stats_cluster_equal, NULL,
                                             (GDestroyNotify) stats_cluster_free);

  g_static_mutex_init(&stats_mutex);
}

void
stats_registry_deinit(void)
{
  g_hash_table_destroy(stats_cluster_container.static_clusters);
  g_hash_table_destroy(stats_cluster_container.dynamic_clusters);
  stats_cluster_container.static_clusters = NULL;
  stats_cluster_container.dynamic_clusters = NULL;
  g_static_mutex_free(&stats_mutex);
}

