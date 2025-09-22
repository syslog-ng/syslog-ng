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
#include "stats/stats-prometheus.h"
#include "stats/stats-counter.h"
#include "stats/stats-registry.h"
#include "stats/stats-cluster.h"
#include "stats/aggregator/stats-aggregator-registry.h"
#include "stats/stats-query-commands.h"
#include "control/control-commands.h"
#include "control/control-server.h"
#include "control/control-connection.h"

#include <string.h>


static inline void
_reset_counter_if_needed(StatsCluster *sc, gint type, StatsCounterItem *counter, gpointer user_data)
{
  stats_cluster_reset_counter_if_needed(sc, counter);
}

static void
_reset_counters(void)
{
  stats_lock();
  stats_foreach_counter(_reset_counter_if_needed, NULL, NULL);
  stats_unlock();
  stats_aggregator_lock();
  stats_aggregator_registry_reset();
  stats_aggregator_unlock();
}

static void
_send_batched_response(const gchar *record, gpointer user_data)
{
  static const gsize BATCH_LEN = 2048;

  gpointer *args = (gpointer *) user_data;
  ControlConnection *cc = (ControlConnection *) args[0];
  GString **batch = (GString **) args[1];

  if (!*batch)
    *batch = g_string_sized_new(512);
  g_string_append_printf(*batch, "%s", record);

  if ((*batch)->len > BATCH_LEN)
    {
      control_connection_send_batched_reply(cc, *batch);
      *batch = NULL;
    }
}

static void
control_connection_send_stats(ControlConnection *cc, GString *command, gpointer user_data, gboolean *cancelled)
{
  gchar **cmds = g_strsplit(command->str, " ", 3);
  gsize cmd_ndx = 0;
  g_assert(g_str_equal(cmds[cmd_ndx], "STATS"));
  ++cmd_ndx;
  g_assert(cmds[cmd_ndx] != NULL && "STATS command must have at least one format argument");

  GString *response = NULL;
  gpointer args[] = {cc, &response};

  if (strcmp(cmds[cmd_ndx], "PROMETHEUS") == 0)
    {
      gboolean with_legacy = g_strcmp0(cmds[cmd_ndx + 1], "WITH_LEGACY") == 0;
      if (with_legacy)
        ++cmd_ndx;
      gboolean without_orphaned = g_strcmp0(cmds[cmd_ndx + 1], "WITHOUT_ORPHANED") == 0;
      // NOTE: do not forget to increment cmd_ndx if new commands added later
      //       now just commented out to avoid compiler warning
      // if (without_orphaned)
      //   ++cmd_ndx;
      stats_generate_prometheus(_send_batched_response, args, with_legacy, without_orphaned, cancelled);
    }
  else
    {
      gboolean csv = strcmp(cmds[cmd_ndx], "CSV") == 0;
      g_assert(csv || strcmp(cmds[cmd_ndx], "KV") == 0);
      gboolean without_header = g_strcmp0(cmds[cmd_ndx + 1], "WITHOUT_HEADER") == 0;
      if (without_header)
        ++cmd_ndx;
      gboolean without_orphaned = g_strcmp0(cmds[cmd_ndx + 1], "WITHOUT_ORPHANED") == 0;
      // NOTE: do not forget to increment cmd_ndx if new commands added later
      //       now just commented out to avoid compiler warning
      // if (without_orphaned)
      //   ++cmd_ndx;
      stats_generate_csv_or_kv(_send_batched_response, args, csv, FALSE == without_header, without_orphaned, cancelled);
    }

  if (response != NULL)
    control_connection_send_batched_reply(cc, response);
  control_connection_send_close_batch(cc);

  g_strfreev(cmds);
}

static void
control_connection_reset_stats(ControlConnection *cc, GString *command, gpointer user_data, gboolean *cancelled)
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
control_connection_remove_orphans(ControlConnection *cc, GString *command, gpointer user_data, gboolean *cancelled)
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
  control_register_command("STATS", control_connection_send_stats, NULL, TRUE);
  control_register_command("RESET_STATS", control_connection_reset_stats, NULL, FALSE);
  control_register_command("REMOVE_ORPHANED_STATS", control_connection_remove_orphans, NULL, FALSE);
  control_register_command("QUERY", stats_process_query_command, NULL, TRUE);
}
