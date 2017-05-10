/*
 * Copyright (c) 2017 Balabit
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

#include "messages.h"
#include "stats-query.h"
#include "stats-registry.h"
#include "stats.h"

#include <string.h>


static GHashTable *counter_index;
static GStaticMutex stats_query_mutex = G_STATIC_MUTEX_INIT;
static GHashTable *stats_views;

typedef struct _ViewRecord
{
  GList *queries;
  StatsCounterItem *counter;
  AggregatedMetricsCb aggregate;
} ViewRecord;

void
_free_view_record(gpointer r)
{
  ViewRecord *record = (ViewRecord *) r;
  g_list_free_full(record->queries, g_free);
  g_free(record);
}


static void
_setup_filter_expression(const gchar *expr, gchar **key_str)
{
  if (!expr || g_str_equal(expr, ""))
    {
      *key_str = g_strdup("*");
    }
  else
    {
      *key_str = g_strdup(expr);
    }
}

static gchar *
_construct_counter_item_name(StatsCluster *sc, gint type)
{
  GString *name = g_string_new("");

  g_string_append(name, sc->query_key);
  g_string_append(name, ".");
  g_string_append(name, stats_cluster_get_type_name(sc, type));

  return g_string_free(name, FALSE);
}

static void
_add_counter_to_index(StatsCluster *sc, gint type)
{
  StatsCounterItem *counter = &sc->counter_group.counters[type];
  gchar *counter_full_name = NULL;

  counter_full_name = stats_counter_get_name(counter);
  if (counter_full_name == NULL)
    {
      counter_full_name = _construct_counter_item_name(sc, type);
      counter->name = counter_full_name;
    }

  g_hash_table_insert(counter_index, counter_full_name, counter);
  sc->indexed_mask |= type;
}

static void
_remove_counter_from_index(StatsCluster *sc, gint type)
{
  gchar *key = stats_counter_get_name(&sc->counter_group.counters[type]);
  g_hash_table_remove(counter_index, key);
  sc->indexed_mask |= type;
}

static void
_index_counter(StatsCluster *sc, gint type, StatsCounterItem *counter, gpointer user_data)
{
  if (!stats_cluster_is_indexed(sc, type) && stats_cluster_is_alive(sc, type))
    {
      _add_counter_to_index(sc, type);
    }
  else if (stats_cluster_is_indexed(sc, type) && !stats_cluster_is_alive(sc, type))
    {
      _remove_counter_from_index(sc, type);
    }
}

static void
_update_indexes_of_cluster_if_needed(gpointer key, gpointer value)
{
  StatsCluster *sc = (StatsCluster *)key;
  stats_cluster_foreach_counter(sc, _index_counter, NULL);
}

static void
_update_index(void)
{
  GHashTable *counter_container = stats_registry_get_container();
  gpointer key, value;
  GHashTableIter iter;

  g_static_mutex_lock(&stats_query_mutex);
  g_hash_table_iter_init(&iter, counter_container);
  while (g_hash_table_iter_next(&iter, &key, &value))
    {
      _update_indexes_of_cluster_if_needed(key, value);
    }
  g_static_mutex_unlock(&stats_query_mutex);
}

static gboolean
_is_pattern_matches_key(GPatternSpec *pattern, gpointer key)
{
  gchar *counter_name = (gchar *) key;
  return g_pattern_match_string(pattern, counter_name);
}

static gboolean
_is_single_match(gchar *key_str)
{
  gboolean is_wildcard = strchr(key_str, '*') ? TRUE : FALSE;
  gboolean is_joker = strchr(key_str, '?') ? TRUE : FALSE;

  return !is_wildcard && !is_joker;
}

static GList *
_query_counter_hash(gchar *key_str)
{
  GPatternSpec *pattern = g_pattern_spec_new(key_str);
  GList *counters = NULL;
  gpointer key, value;
  GHashTableIter iter;
  gboolean single_match;

  _update_index();
  single_match = _is_single_match(key_str);

  g_static_mutex_lock(&stats_query_mutex);
  g_hash_table_iter_init(&iter, counter_index);
  while (g_hash_table_iter_next(&iter, &key, &value))
    {
      if (_is_pattern_matches_key(pattern, key))
        {
          StatsCounterItem *counter = (StatsCounterItem *) value;
          counters = g_list_append(counters, counter);

          if (single_match)
            break;
        }
    }
  g_static_mutex_unlock(&stats_query_mutex);

  g_pattern_spec_free(pattern);
  return counters;
}

static void
_format_selected_counters(GList *counters, StatsFormatCb format_cb, gpointer result)
{
  for (GList *counter = counters; counter; counter = counter->next)
    {
      StatsCounterItem *c = counter->data;
      format_cb(c, result);
    }
}

static void
_reset_selected_counters(GList *counters)
{
  GList *c;
  for (c = counters; c; c = c->next)
    {
      StatsCounterItem *counter = c->data;
      stats_counter_set(counter, 0);
    }
}

gboolean
_stats_query_get(const gchar *expr, StatsFormatCb format_cb, gpointer result, gboolean must_reset)
{
  if (!expr)
    return FALSE;

  gchar *key_str = NULL;
  GList *counters = NULL;
  gboolean found_match = FALSE;

  _setup_filter_expression(expr, &key_str);
  counters = _query_counter_hash(key_str);
  _format_selected_counters(counters, format_cb, result);

  if (must_reset)
    _reset_selected_counters(counters);

  if (g_list_length(counters) > 0)
    found_match = TRUE;

  g_free(key_str);

  return found_match;
}

gboolean
stats_query_get(const gchar *expr, StatsFormatCb format_cb, gpointer result)
{
  return _stats_query_get(expr, format_cb, result, FALSE);
}

gboolean
stats_query_get_and_reset_counters(const gchar *expr, StatsFormatCb format_cb, gpointer result)
{
  return _stats_query_get(expr, format_cb, result, TRUE);
}

static gboolean
_is_timestamp(gchar *counter_name)
{
  gchar *last_dot = strrchr(counter_name, '.');
  return (g_strcmp0(last_dot, ".stamp") == 0);
}

void
_sum_selected_counters(GList *counters, gpointer user_data)
{
  GList *c;
  gpointer *args = (gpointer *) user_data;
  gint64 *sum = (gint64 *) args[1];
  for (c = counters; c; c = c->next)
    {
      StatsCounterItem *counter = c->data;
      if (!_is_timestamp(stats_counter_get_name(counter)))
        *sum += stats_counter_get(counter);
    }
}

gboolean
_stats_query_get_sum(const gchar *expr, StatsFormatCb format_cb, gpointer result, gboolean must_reset)
{
  if (!expr)
    return FALSE;

  gchar *key_str = NULL;
  GList *counters = NULL;
  gboolean found_match = FALSE;
  gint64 sum = 0;
  gpointer args[] = {result, &sum};

  _setup_filter_expression(expr, &key_str);
  counters = _query_counter_hash(key_str);
  _sum_selected_counters(counters, (gpointer)args);
  _format_selected_counters(counters, format_cb, (gpointer)args);

  if (must_reset)
    _reset_selected_counters(counters);

  if (g_list_length(counters) > 0)
    found_match = TRUE;

  g_free(key_str);

  return found_match;
}

gboolean
stats_query_get_sum(const gchar *expr, StatsFormatCb format_cb, gpointer result)
{
  return _stats_query_get_sum(expr, format_cb, result, FALSE);
}

gboolean
stats_query_get_sum_and_reset_counters(const gchar *expr, StatsFormatCb format_cb, gpointer result)
{
  return _stats_query_get_sum(expr, format_cb, result, TRUE);
}

gboolean
_stats_query_list(const gchar *expr, StatsFormatCb format_cb, gpointer result, gboolean must_reset)
{
  gchar *key_str = NULL;
  GList *counters = NULL;
  gboolean found_match = FALSE;

  _setup_filter_expression(expr, &key_str);
  counters = _query_counter_hash(key_str);
  _format_selected_counters(counters, format_cb, result);

  if (must_reset)
    _reset_selected_counters(counters);

  if (g_list_length(counters) > 0)
    found_match = TRUE;

  g_free(key_str);

  return found_match;
}

gboolean
stats_query_list(const gchar *expr, StatsFormatCb format_cb, gpointer result)
{
  return _stats_query_list(expr, format_cb, result, FALSE);
}

gboolean
stats_query_list_and_reset_counters(const gchar *expr, StatsFormatCb format_cb, gpointer result)
{
  return _stats_query_list(expr, format_cb, result, TRUE);
}

void
stats_query_init(void)
{
  counter_index = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
  stats_views = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _free_view_record);
}

void
stats_query_deinit(void)
{
  g_hash_table_destroy(counter_index);
  counter_index = NULL;
  g_hash_table_destroy(stats_views);
  stats_views = NULL;
}
