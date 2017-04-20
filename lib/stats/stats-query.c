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

typedef struct _NamedStatsCounterItem
{
  StatsCluster *cluster;
  gint index;
} NamedStatsCounterItem;

typedef gboolean (*StatsClusterCounterCb)(StatsCluster *sc, NamedStatsCounterItem *named_counter,
                                          StatsFormatCb format_cb, gpointer user_data);


static const gchar *
_get_counter_name_by_index(gint type_index)
{
  const gchar *_counter_type[SC_TYPE_MAX+1] =
  {
    "dropped",
    "processed",
    "stored",
    "suppressed",
    "stamp",
    "MAX"
  };

  if (type_index > SC_TYPE_MAX)
    return "";

  return _counter_type[type_index];
}

static const gchar *
_get_name_of_counter_item(NamedStatsCounterItem *item)
{
  return _get_counter_name_by_index(item->index);
}

static void
_append_live_counters(StatsCluster *sc, gchar *ctr_str, GList **counters)
{
  gint i;
  gboolean is_wildcard = FALSE;
  if (ctr_str[0] == '*')
    is_wildcard = TRUE;

  for (i = 0; i < SC_TYPE_MAX; i++)
    {
      if (stats_cluster_is_alive(sc, i))
        {
          if (is_wildcard || !strcmp(_get_counter_name_by_index(i), ctr_str))
            {
              NamedStatsCounterItem *c = g_new0(NamedStatsCounterItem, 1);
              c->cluster = sc;
              c->index = i;
              *counters = g_list_append(*counters, c);
            }
        }
    }
}

void
_split_expr(const gchar *expr, gchar **key, gchar **ctr)
{
  const gchar *last_delim_pos = strrchr(expr, '.');
  size_t expr_len = strlen(expr);
  if (last_delim_pos)
    {
      size_t key_len = last_delim_pos - expr ;
      *key = g_strndup(expr, key_len);
      *ctr = g_strndup(expr + key_len + 1, expr_len - key_len - 1);
    }
  else
    {
      *key = g_strdup(expr);
      *ctr = g_strdup("*");
    }
}

void
_setup_filter_expression(const gchar *expr, gchar **key_str, gchar **ctr_str)
{
  if (!expr)
    {
      *key_str = g_strdup("*");
      *ctr_str = g_strdup("*");
    }
  else
    {
      _split_expr(expr, key_str, ctr_str);
    }
}

static gboolean
_find_key_by_pattern(gpointer key, gpointer value, gpointer user_data)
{
  StatsCluster *sc = (StatsCluster *)key;
  GPatternSpec *pattern = (GPatternSpec *)user_data;

  return g_pattern_match_string(pattern, sc->query_key);
}

static GList *
_query_counter_hash(gchar *key_str, gchar *ctr_str)
{
  GHashTable *counter_hash = stats_registry_get_container();
  GPatternSpec *pattern = g_pattern_spec_new(key_str);
  StatsCluster *sc = NULL;
  GList *counters = NULL;
  gpointer key, value;
  GHashTableIter iter;

  g_hash_table_iter_init (&iter, counter_hash);

  gboolean single_match = strchr(key_str, '*') ? FALSE : TRUE;
  while (g_hash_table_iter_next(&iter, &key, &value))
    {
      if (_find_key_by_pattern(key, value, (gpointer)pattern))
        {
          sc = (StatsCluster *)key;
          _append_live_counters(sc, ctr_str, &counters);

          if (single_match)
            break;
        }
    }
  g_pattern_spec_free(pattern);
  return counters;
}

static void
_format_selected_counters(GList *counters, StatsFormatCb format_cb, gpointer result)
{
  for (GList *counter = counters; counter; counter = counter->next)
    {
      NamedStatsCounterItem *c = counter->data;
      format_cb(c->cluster, &c->cluster->counter_group.counters[c->index], _get_name_of_counter_item(c), result);
    }
}

static void
_reset_selected_counters(GList *counters)
{
  GList *c;
  for (c = counters; c; c = c->next)
    {
      NamedStatsCounterItem *counter = c->data;
      StatsCounterItem *item = &counter->cluster->counter_group.counters[counter->index];
      stats_counter_set(item, 0);
    }
}

gboolean
_stats_query_get(const gchar *expr, StatsFormatCb format_cb, gpointer result, gboolean must_reset)
{
  if (!expr)
    return FALSE;

  gchar *key_str = NULL;
  gchar *ctr_str = NULL;
  GList *counters = NULL;
  gboolean found_match = FALSE;

  _split_expr(expr, &key_str, &ctr_str);
  counters = _query_counter_hash(key_str, ctr_str);
  _format_selected_counters(counters, format_cb, result);

  if (must_reset)
    _reset_selected_counters(counters);

  if (g_list_length(counters) > 0)
    found_match = TRUE;

  g_list_free_full(counters, g_free);
  g_free(key_str);
  g_free(ctr_str);

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

void
_sum_selected_counters(GList *counters, gpointer user_data)
{
  GList *c;
  gpointer *args = (gpointer *) user_data;
  gint64 *sum = (gint64 *) args[1];
  for (c = counters; c; c = c->next)
    {
      NamedStatsCounterItem *counter = c->data;
      *sum += counter->cluster->counter_group.counters[counter->index].value;
    }
}

gboolean
_stats_query_get_sum(const gchar *expr, StatsFormatCb format_cb, gpointer result, gboolean must_reset)
{
  gchar *key_str = NULL;
  gchar *ctr_str = NULL;
  GList *counters = NULL;
  gboolean found_match = FALSE;
  gint64 sum = 0;
  gpointer args[] = {result, &sum};

  _setup_filter_expression(expr, &key_str, &ctr_str);
  counters = _query_counter_hash(key_str, ctr_str);
  _sum_selected_counters(counters, (gpointer)args);
  _format_selected_counters(counters, format_cb, (gpointer)args);

  if (must_reset)
    _reset_selected_counters(counters);

  if (g_list_length(counters) > 0)
    found_match = TRUE;

  g_list_free_full(counters, g_free);
  g_free(key_str);
  g_free(ctr_str);

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
  gchar *ctr_str = NULL;
  GList *counters = NULL;
  gboolean found_match = FALSE;

  _setup_filter_expression(expr, &key_str, &ctr_str);
  counters = _query_counter_hash(key_str, ctr_str);
  _format_selected_counters(counters, format_cb, result);

  if (must_reset)
    _reset_selected_counters(counters);

  if (g_list_length(counters) > 0)
    found_match = TRUE;

  g_list_free_full(counters, g_free);
  g_free(key_str);
  g_free(ctr_str);

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
