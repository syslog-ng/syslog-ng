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

static const gchar *_counter_type[SC_TYPE_MAX+1] =
{
  "dropped",
  "processed",
  "stored",
  "suppressed",
  "stamp",
  "MAX"
};


typedef gboolean (*StatsClusterCounterCb)(StatsCluster *sc, StatsCounterItem *ctr, const gchar *ctr_name,
                                          StatsFormatCb format_cb, gpointer user_data);

static void
_foreach_live_counters(StatsCluster *sc, StatsClusterCounterCb counter_cb, StatsFormatCb format_cb, gpointer user_data)
{
  for (gint i = 0; i < SC_TYPE_MAX; i++)
    {
      if (stats_cluster_is_alive(sc, i))
        if (!counter_cb(sc, &sc->counters[i], _counter_type[i], format_cb, user_data))
          break;
    }
}

static gboolean
_append_counter_with_value(StatsCluster *sc, StatsCounterItem *ctr, const gchar *ctr_name, StatsFormatCb format_cb,
                           gpointer user_data)
{
  return format_cb(sc, ctr, ctr_name, user_data);
}

static gboolean
_append_counter_without_value(StatsCluster *sc, StatsCounterItem *ctr, const gchar *ctr_name, StatsFormatCb format_cb,
                              gpointer user_data)
{
  return format_cb(sc, ctr, ctr_name, user_data);
}

static gboolean
_append_counter_with_value_when_name_is_matching(StatsCluster *sc, StatsCounterItem *ctr, const gchar *ctr_name,
                                                 StatsFormatCb format_cb, gpointer user_data)
{
  gpointer *args = (gpointer *) user_data;
  GString *result = (GString *) args[0];
  const gchar *requested_ctr_name = (const gchar *) args[1];
  if (ctr_name && !strcmp(requested_ctr_name, ctr_name))
    {
      _append_counter_with_value(sc, ctr, ctr_name, format_cb, (gpointer) result);
      return FALSE;
    }
  return TRUE;
}

static gboolean
_append_counter_without_value_when_name_is_matching(StatsCluster *sc, StatsCounterItem *ctr, const gchar *ctr_name,
                                                    StatsFormatCb format_cb,
                                                    gpointer user_data)
{
  gpointer *args = (gpointer *) user_data;
  GString *result = (GString *) args[0];
  const gchar *requested_ctr_name = (const gchar *) args[1];
  if (ctr_name && !strcmp(requested_ctr_name, ctr_name))
    {
      _append_counter_without_value(sc, ctr, ctr_name, format_cb, (gpointer) result);
      return FALSE;
    }
  return TRUE;
}

static gboolean
_add_counter_value(StatsCluster *sc, StatsCounterItem *ctr, const gchar *ctr_name, StatsFormatCb format_cb,
                   gpointer user_data)
{
  if (ctr_name && g_str_equal(ctr_name, _counter_type[SC_TYPE_STAMP]))
    return TRUE;

  gpointer *args = (gpointer *) user_data;
  gint64 *sum = (gint64 *) args[1];

  *sum += ctr->value;

  return format_cb(sc, ctr, ctr_name, (gpointer)args);
}

static gboolean
_add_value_when_name_is_matching(StatsCluster *sc, StatsCounterItem *ctr, const gchar *ctr_name,
                                 StatsFormatCb format_cb, gpointer user_data)
{
  gpointer *args = (gpointer *) user_data;
  const gchar *requested_ctr_name = (const gchar *) args[1];

  if (ctr_name && g_str_equal(ctr_name, _counter_type[SC_TYPE_STAMP]))
    return FALSE;

  if (ctr_name && !strcmp(requested_ctr_name, ctr_name))
    {
      _add_counter_value(sc, ctr, ctr_name, format_cb, (gpointer)args[0]);
      return FALSE;
    }
  return TRUE;
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

static gboolean
_query_counter_hash(gchar *key_str, gchar *ctr_str, StatsClusterCounterCb wildcard_match_cb,
                    StatsClusterCounterCb match_cb, StatsFormatCb format_cb, gpointer result)
{
  GHashTable *counter_hash = stats_registry_get_counter_hash();
  GPatternSpec *pattern = g_pattern_spec_new(key_str);
  StatsCluster *sc = NULL;
  gboolean found_match = FALSE;
  gpointer key, value;
  GHashTableIter iter;

  g_hash_table_iter_init (&iter, counter_hash);

  gboolean single_match = strchr(key_str, '*') ? FALSE : TRUE;
  while (g_hash_table_iter_next(&iter, &key, &value))
    {
      if (_find_key_by_pattern(key, value, (gpointer)pattern))
        {
          sc = (StatsCluster *)key;
          if (ctr_str[0] == '*')
            {
              _foreach_live_counters(sc, wildcard_match_cb, format_cb, result);
            }
          else
            {
              gpointer args[] = {result, (gpointer) ctr_str};
              _foreach_live_counters(sc, match_cb, format_cb, (gpointer) args);
            }
          found_match = TRUE;
          if (single_match)
            break;
        }
    }
  g_pattern_spec_free(pattern);
  return found_match;
}

gboolean
stats_query_get(const gchar *expr, StatsFormatCb format_cb, gpointer result)
{
  gboolean found_match;

  if (!expr)
    return FALSE;

  gchar *key_str = NULL;
  gchar *ctr_str = NULL;

  _split_expr(expr, &key_str, &ctr_str);
  found_match = _query_counter_hash(key_str, ctr_str, _append_counter_with_value,
                                    _append_counter_with_value_when_name_is_matching,
                                    format_cb, result);

  g_free(key_str);
  g_free(ctr_str);

  return found_match;
}

gboolean
stats_query_get_sum(const gchar *expr, StatsFormatCb format_cb, gpointer result)
{
  gboolean found_match;
  gchar *key_str = NULL;
  gchar *ctr_str = NULL;
  gint64 sum = 0;
  gpointer args[] = {result, (gpointer) &sum};

  _setup_filter_expression(expr, &key_str, &ctr_str);
  found_match = _query_counter_hash(key_str, ctr_str, _add_counter_value, _add_value_when_name_is_matching, format_cb,
                                    (gpointer) args);

  g_free(key_str);
  g_free(ctr_str);

  return found_match;
}

gboolean
stats_query_list(const gchar *expr, StatsFormatCb format_cb, gpointer result)
{
  gboolean found_match;
  gchar *key_str = NULL;
  gchar *ctr_str = NULL;

  _setup_filter_expression(expr, &key_str, &ctr_str);
  found_match = _query_counter_hash(key_str, ctr_str, _append_counter_without_value,
                                    _append_counter_without_value_when_name_is_matching, format_cb, result);

  g_free(key_str);
  g_free(ctr_str);

  return found_match;
}
