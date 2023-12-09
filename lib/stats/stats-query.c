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
#include "stats/stats-control.h"
#include "stats.h"

#include <string.h>

typedef void (*ProcessCounterCb)(StatsCounterItem *counter, gpointer user_data, StatsFormatCb format_cb,
                                 gpointer result);

static const gchar *
_setup_filter_expression(const gchar *expr)
{
  if (!expr || g_str_equal(expr, ""))
    return "*";
  else
    return expr;
}

static gboolean
_is_pattern_matches_key(GPatternSpec *pattern, gpointer key)
{
  gchar *counter_name = (gchar *) key;
  return g_pattern_spec_match_string(pattern, counter_name);
}

static gboolean
_is_single_match(const gchar *key_str)
{
  return !strpbrk(key_str, "*?");
}

static void
_process_counter_if_matching(StatsCluster *sc, gint type, StatsCounterItem *counter,
                             gpointer user_data /*, gboolean* cancelled*/)
{
  gpointer *args = (gpointer *) user_data;
  int arg_ndx = 0;
  GPatternSpec *pattern = (GPatternSpec *) args[arg_ndx++];
  gboolean single_match = *(gboolean const *) args[arg_ndx++];
  ProcessCounterCb process_func = (ProcessCounterCb) args[arg_ndx++];
  gpointer process_func_user_data = args[arg_ndx++];
  StatsFormatCb format_cb = (StatsFormatCb) args[arg_ndx++];
  gpointer format_cb_user_data = args[arg_ndx++];
  gboolean must_reset = *(gboolean const *) args[arg_ndx++];
  gboolean *found = (gboolean *) args[arg_ndx++];

  if (stats_cluster_is_alive(sc, type))
    {
      if (_is_pattern_matches_key(pattern, counter->name))
        {
          *found = TRUE;

          process_func(counter, process_func_user_data, format_cb, format_cb_user_data);
          if (must_reset)
            stats_cluster_reset_counter_if_needed(sc, counter);

          if (single_match)
            {
              /* TODO: cancelled parameter is not implemented yet in the counter enumerators, only in the cluster ones, but
                       not stopping here now does not make a performace difference as the original impl always interated
                       over all the counter items in all the clusters in multiple rounds anyway
              *cancelled = TRUE;
              */
            }
        }
    }
}

static void
_process_counters(StatsCluster *sc, gpointer user_data /*, gboolean* cancelled */)
{
  stats_cluster_foreach_counter(sc, _process_counter_if_matching, user_data /*, cancelled*/);
}

static gboolean
_process_matching_counters(const gchar *key_str, ProcessCounterCb process_func, gpointer process_func_user_data,
                           StatsFormatCb format_cb, gpointer format_cb_user_data, gboolean must_reset)
{
  gboolean cancelled = FALSE;
  gboolean found = FALSE;
  GPatternSpec *pattern = g_pattern_spec_new(key_str);
  gboolean is_single_match = _is_single_match(key_str);
  gpointer args[] = {pattern, (gpointer) &is_single_match, process_func, process_func_user_data, format_cb, format_cb_user_data, (gpointer) &must_reset, (gpointer) &found};

  stats_lock();
  stats_foreach_cluster(_process_counters, args, &cancelled);
  stats_unlock();

  g_pattern_spec_free(pattern);

  return found;
}

static void
_format_selected_counter(StatsCounterItem *counter, gpointer user_data, StatsFormatCb format_cb, gpointer result)
{
  gpointer args[] = {counter, result};
  format_cb(args);
}

gboolean
_stats_query_get(const gchar *expr, StatsFormatCb format_cb, gpointer result, gboolean must_reset)
{
  if (!expr)
    return FALSE;

  const gchar *key_str = _setup_filter_expression(expr);
  return _process_matching_counters(key_str, _format_selected_counter, NULL, format_cb, result, must_reset);
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
_sum_selected_counters(StatsCounterItem *counter, gpointer user_data, StatsFormatCb format_cb, gpointer result)
{
  gpointer *args = (gpointer *) user_data;
  gint64 *sum = (gint64 *) args[1];

  if (!_is_timestamp(stats_counter_get_name(counter)))
    *sum += stats_counter_get(counter);
}

gboolean
_stats_query_get_sum(const gchar *expr, StatsFormatCb format_cb, gpointer result, gboolean must_reset)
{
  if (!expr)
    return FALSE;

  const gchar *key_str = _setup_filter_expression(expr);
  gint64 sum = 0;
  gpointer args[] = {result, &sum};
  gboolean found = _process_matching_counters(key_str, _sum_selected_counters, args, NULL, NULL, must_reset);

  if (found)
    format_cb(args);
  return found;
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
  const gchar *key_str = _setup_filter_expression(expr);
  return _process_matching_counters(key_str, _format_selected_counter, NULL, format_cb, result, must_reset);
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
