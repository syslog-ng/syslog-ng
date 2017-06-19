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
