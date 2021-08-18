/*
 * Copyright (c) 2002-2017 Balabit
 * Copyright (c) 1998-2017 Balázs Scheidler
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

#include "stats/stats-control.h"
#include "stats/stats-csv.h"
#include "stats/stats-counter.h"
#include "stats/stats-registry.h"
#include "stats/stats-cluster.h"
#include "stats/aggregator/stats-aggregator-registry.h"
#include "stats/stats-query-commands.h"
#include "control/control-commands.h"
#include "control/control-server.h"

static void
_reset_counter(StatsCluster *sc, gint type, StatsCounterItem *counter, gpointer user_data)
{
  stats_counter_set(counter, 0);
}

static inline void
_reset_counter_if_needed(StatsCluster *sc, gint type, StatsCounterItem *counter, gpointer user_data)
{
  if (stats_counter_read_only(counter))
    return;

  switch (type)
    {
    case SC_TYPE_QUEUED:
    case SC_TYPE_MEMORY_USAGE:
      return;
    default:
      _reset_counter(sc, type, counter, user_data);
    }
}

static void
_reset_counters(void)
{
  stats_lock();
  stats_foreach_counter(_reset_counter_if_needed, NULL);
  stats_unlock();
  stats_aggregator_lock();
  stats_aggregator_registry_reset();
  stats_aggregator_unlock();
}

static GString *
_send_stats_get_result(ControlConnection *cc, GString *command, gpointer user_data)
{
  gchar *stats = stats_generate_csv();
  GString *response = g_string_new(stats);
  g_free(stats);

  return response;
}

static void
control_connection_send_stats(ControlConnection *cc, GString *command, gpointer user_data)
{
  control_connection_start_as_thread(cc, _send_stats_get_result, command, user_data);
}

static void
control_connection_reset_stats(ControlConnection *cc, GString *command, gpointer user_data)
{
  GString *result = g_string_new("OK The statistics of syslog-ng have been reset to 0.");
  _reset_counters();
  control_connection_send_reply(cc, result);
}

static gboolean
_is_cluster_orphaned(StatsCluster *sc, gpointer user_data)
{
  return stats_cluster_is_orphaned(sc);
}

static void
control_connection_remove_orphans(ControlConnection *cc, GString *command, gpointer user_data)
{
  GString *result = g_string_new("OK Orphaned statistics have been removed.");

  stats_aggregator_lock();
  stats_aggregator_remove_orphaned_stats();
  stats_aggregator_unlock();
  stats_lock();
  stats_foreach_cluster_remove(_is_cluster_orphaned, NULL);
  stats_unlock();

  control_connection_send_reply(cc, result);
}

void
stats_register_control_commands(void)
{
  control_register_command("STATS", control_connection_send_stats, NULL);
  control_register_command("RESET_STATS", control_connection_reset_stats, NULL);
  control_register_command("REMOVE_ORPHANED_STATS", control_connection_remove_orphans, NULL);
  control_register_command("QUERY", process_query_command, NULL);
}
