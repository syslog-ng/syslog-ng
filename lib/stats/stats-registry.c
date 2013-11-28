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

static gboolean
stats_counter_equal(gconstpointer p1, gconstpointer p2)
{
  const StatsCluster *sc1 = (StatsCluster *) p1;
  const StatsCluster *sc2 = (StatsCluster *) p2;
  
  return sc1->component == sc2->component && strcmp(sc1->id, sc2->id) == 0 && strcmp(sc1->instance, sc2->instance) == 0;
}

static guint
stats_counter_hash(gconstpointer p)
{
  const StatsCluster *sc = (StatsCluster *) p;
  
  return g_str_hash(sc->id) + g_str_hash(sc->instance) + sc->component;
}

static void
stats_counter_free(gpointer p)
{ 
  StatsCluster *sc = (StatsCluster *) p;
  
  g_free(sc->id);
  g_free(sc->instance);
  g_free(sc);
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
stats_add_counter(gint stats_level, gint component, const gchar *id, const gchar *instance, gboolean *new)
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
      sc = g_new0(StatsCluster, 1);
      
      sc->component = component;
      sc->id = g_strdup(id);
      sc->instance = g_strdup(instance);
      sc->ref_cnt = 1;
      g_hash_table_insert(counter_hash, sc, sc);
      *new = TRUE;
    }
  else
    {
      if (sc->ref_cnt == 0)
        /* it just haven't been cleaned up */
        *new = TRUE;
      else
        *new = FALSE;

      sc->ref_cnt++;
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
  gboolean new;

  g_assert(stats_locked);
  g_assert(type < SC_TYPE_MAX);
  
  *counter = NULL;
  sc = stats_add_counter(stats_level, component, id, instance, &new);
  if (!sc)
    return;

  *counter = &sc->counters[type];
  sc->live_mask |= 1 << type;
}

StatsCluster *
stats_register_dynamic_counter(gint stats_level, gint component, const gchar *id, const gchar *instance, StatsCounterType type, StatsCounterItem **counter, gboolean *new)
{
  StatsCluster *sc;
  gboolean local_new;

  g_assert(stats_locked);
  g_assert(type < SC_TYPE_MAX);
  
  *counter = NULL;
  *new = FALSE;
  sc = stats_add_counter(stats_level, component, id, instance, &local_new);
  if (new)
    *new = local_new;
  if (!sc)
    return NULL;

  if (!local_new && !sc->dynamic)
    g_assert_not_reached();

  sc->dynamic = TRUE;
  *counter = &sc->counters[type];
  sc->live_mask |= 1 << type;
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
  gboolean new;
  StatsCluster *handle;

  g_assert(stats_locked);
  handle = stats_register_dynamic_counter(stats_level, component, id, instance, SC_TYPE_PROCESSED, &counter, &new);
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

  *counter = &sc->counters[type];
  sc->live_mask |= 1 << type;
  sc->ref_cnt++;
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

  g_assert(sc && (sc->live_mask & (1 << type)) && &sc->counters[type] == (*counter));
  
  *counter = NULL;
  sc->ref_cnt--;
}

void
stats_unregister_dynamic_counter(StatsCluster *sc, StatsCounterType type, StatsCounterItem **counter)
{
  g_assert(stats_locked);
  if (!sc)
    return;
  g_assert(sc && (sc->live_mask & (1 << type)) && &sc->counters[type] == (*counter));
  sc->ref_cnt--;
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
  counter_hash = g_hash_table_new_full(stats_counter_hash, stats_counter_equal, NULL, stats_counter_free);
  g_static_mutex_init(&stats_mutex);
}

void
stats_registry_deinit(void)
{
  g_hash_table_destroy(counter_hash);
  counter_hash = NULL;
  g_static_mutex_free(&stats_mutex);
}
