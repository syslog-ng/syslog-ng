/*
 * Copyright (c) 2017 Balabit
 * Copyright (c) 2017 Noemi Vanyi
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


#include "stats-views.h"
#include "stats-cluster-single.h"

static gchar *
_construct_view_name(StatsCluster *cluster, gchar *view_name)
{
  GString *name = g_string_new(cluster->query_key);
  name = g_string_append(name, ".");
  name = g_string_append(name, view_name);
  return g_string_free(name, FALSE);
}

static void
_calculate_written_messages(GList *counters, StatsCounterItem **result)
{
  StatsCounterItem *processed = NULL, *dropped = NULL, *queued = NULL;
  gssize written = 0;

  for (GList *c = counters; c; c = c->next)
    {
      StatsCounterItem *counter = c->data;
      if (counter->type == SC_TYPE_PROCESSED)
        processed = counter;
      else if (counter->type == SC_TYPE_DROPPED)
        dropped = counter;
      else if (counter->type == SC_TYPE_QUEUED)
        queued = counter;
    }

  g_assert(processed != NULL || dropped != NULL || queued != NULL);


  written = stats_counter_get(processed);
  if (written == 0)
    {
      stats_counter_set(*result, written);
      return;
    }

  written -= stats_counter_get(dropped);
  written -= stats_counter_get(queued);
  stats_counter_set(*result, written);
}

static void
_index_required_counters(StatsCluster *cluster, gint *counter_types, gint counter_count)
{
  for (gint i = 0; i < counter_count; i++)
    stats_query_index_counter(cluster, counter_types[i]);
}

void
stats_register_written_view(StatsCluster *cluster, StatsCounterItem *processed, StatsCounterItem *dropped,
                            StatsCounterItem *queued)
{
  GList *written_query = NULL;
  gint counter_types[] = { SC_TYPE_QUEUED, SC_TYPE_DROPPED, SC_TYPE_PROCESSED };
  gsize counter_types_len = sizeof(counter_types) / sizeof(counter_types[0]);
  gchar *written_view_name;

  written_view_name = _construct_view_name(cluster, "written");
  _index_required_counters(cluster, counter_types, counter_types_len);

  written_query = g_list_append(written_query, g_strdup(stats_counter_get_name(queued)));
  written_query = g_list_append(written_query, g_strdup(stats_counter_get_name(processed)));
  written_query = g_list_append(written_query, g_strdup(stats_counter_get_name(dropped)));

  stats_register_view(written_view_name, written_query, _calculate_written_messages);
}

void
_calculate_fifo_fillup_rate(GList *counters, StatsCounterItem **result)
{
  StatsCounterItem *number_of_messages_in_queue = NULL, *queue_capacity = NULL;
  gint rate = 0;

  for (GList *c = counters; c; c = c->next)
    {
      StatsCounterItem *counter = c->data;
      if (counter->type == SC_TYPE_SINGLE_VALUE && g_str_has_suffix(stats_counter_get_name(counter), ".mem_capacity_count"))
        queue_capacity = counter;
      else if (counter->type == SC_TYPE_QUEUED)
        number_of_messages_in_queue = counter;
    }

  g_assert(number_of_messages_in_queue != NULL || queue_capacity != NULL);

  rate = stats_counter_get(number_of_messages_in_queue) * 100 / stats_counter_get(queue_capacity);
  stats_counter_set(*result, rate);
}

void
stats_register_fifo_fillup_rate(StatsCluster *cluster_of_actual, StatsCluster *cluster_of_max, StatsCounterItem *actual,
                                StatsCounterItem *max)
{
  GList *fillup_rate_query = NULL;
  gint counter_type_of_actual[] = { SC_TYPE_QUEUED };
  gint counter_type_of_max[] = { SC_TYPE_SINGLE_VALUE };

  g_assert(actual != NULL || max != NULL || cluster_of_actual != NULL || cluster_of_max != NULL);

  gchar *fillup_rate_name = _construct_view_name(cluster_of_actual, "mem_fillup_rate");
  _index_required_counters(cluster_of_actual, counter_type_of_actual, 1);
  _index_required_counters(cluster_of_max, counter_type_of_max, 1);

  fillup_rate_query = g_list_append(fillup_rate_query, g_strdup(stats_counter_get_name(actual)));
  fillup_rate_query = g_list_append(fillup_rate_query, g_strdup(stats_counter_get_name(max)));

  stats_register_view(fillup_rate_name, fillup_rate_query, _calculate_fifo_fillup_rate);
}
