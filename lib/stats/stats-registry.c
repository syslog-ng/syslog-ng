/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
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

#include <string.h>

static GHashTable *counter_hash;
static GStaticMutex stats_mutex = G_STATIC_MUTEX_INIT;
gboolean stats_locked;

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
_grab_cluster(gint stats_level, gint component, const gchar *id, const gchar *instance)
{
  StatsCluster key;
  StatsCluster *sc;

  if (!stats_check_level(stats_level))
    return NULL;
  
  if (!id)
    id = "";
  if (!instance)
    instance = "";
  
  key.component = component;
  key.id = (gchar *) id;
  key.instance = (gchar *) instance;
  
  sc = g_hash_table_lookup(counter_hash, &key);
  if (!sc)
    {
      /* no such StatsCluster instance, register one */
      sc = stats_cluster_new(component, id, instance);
      g_hash_table_insert(counter_hash, sc, sc);
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
void
stats_register_counter(gint stats_level, gint component, const gchar *id, const gchar *instance, StatsCounterType type, StatsCounterItem **counter)
{
  StatsCluster *sc;

  g_assert(stats_locked);
  
  sc = _grab_cluster(stats_level, component, id, instance);
  if (sc)
    *counter = stats_cluster_track_counter(sc, type);
  else
    *counter = NULL;
}

StatsCluster *
stats_register_dynamic_counter(gint stats_level, gint component, const gchar *id, const gchar *instance, StatsCounterType type, StatsCounterItem **counter)
{
  StatsCluster *sc;

  g_assert(stats_locked);
  
  sc = _grab_cluster(stats_level, component, id, instance);
  if (sc)
    {
      sc->dynamic = TRUE;
      *counter = stats_cluster_track_counter(sc, type);
    }
  else
    {
      *counter = NULL;
    }
  return sc;
}

/*
 * stats_instant_inc_dynamic_counter
 * @timestamp: if non-negative, an associated timestamp will be created and set
 *
 * Instantly create (if not exists) and increment a dynamic counter.
 */
void
stats_register_and_increment_dynamic_counter(gint stats_level, gint component, const gchar *id, const gchar *instance, time_t timestamp)
{
  StatsCounterItem *counter, *stamp;
  StatsCluster *handle;

  g_assert(stats_locked);
  handle = stats_register_dynamic_counter(stats_level, component, id, instance, SC_TYPE_PROCESSED, &counter);
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
stats_register_associated_counter(StatsCluster *sc, StatsCounterType type, StatsCounterItem **counter)
{
  g_assert(stats_locked);

  *counter = NULL;
  if (!sc)
    return;
  g_assert(sc->dynamic);

  *counter = stats_cluster_track_counter(sc, type);
}

void
stats_unregister_counter(gint component, const gchar *id, const gchar *instance, StatsCounterType type, StatsCounterItem **counter)
{
  StatsCluster *sc;
  StatsCluster key;
  
  g_assert(stats_locked);

  if (*counter == NULL)
    return;

  if (!id)
    id = "";
  if (!instance)
    instance = "";
  
  key.component = component;
  key.id = (gchar *) id;
  key.instance = (gchar *) instance;

  sc = g_hash_table_lookup(counter_hash, &key);
  
  stats_cluster_untrack_counter(sc, type, counter);
}

void
stats_unregister_dynamic_counter(StatsCluster *sc, StatsCounterType type, StatsCounterItem **counter)
{
  g_assert(stats_locked);
  if (!sc)
    return;
  stats_cluster_untrack_counter(sc, type, counter);
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
  g_hash_table_foreach(counter_hash, _foreach_cluster_helper, args);
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
  g_hash_table_foreach_remove(counter_hash, _foreach_cluster_remove_helper, args);
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
  counter_hash = g_hash_table_new_full((GHashFunc) stats_cluster_hash, (GEqualFunc) stats_cluster_equal, NULL, (GDestroyNotify) stats_cluster_free);
  g_static_mutex_init(&stats_mutex);
}

void
stats_registry_deinit(void)
{
  g_hash_table_destroy(counter_hash);
  counter_hash = NULL;
  g_static_mutex_free(&stats_mutex);
}
