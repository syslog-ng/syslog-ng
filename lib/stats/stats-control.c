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
#include "stats/stats-query-commands.h"
#include "control/control-commands.h"

static GString *
control_connection_send_stats(ControlConnection *cc, GString *command, gpointer user_data)
{
  gchar *stats = stats_generate_csv();
  GString *result = g_string_new(stats);
  g_free(stats);
  return result;
}

static GString *
control_connection_reset_stats(ControlConnection *cc, GString *command, gpointer user_data)
{
  GString *result = g_string_new("OK The statistics of syslog-ng have been reset to 0.");
  stats_reset_counters();
  return result;
}

void
stats_register_control_commands(void)
{
  control_register_command("STATS", control_connection_send_stats, NULL);
  control_register_command("RESET_STATS", control_connection_reset_stats, NULL);
  control_register_command("QUERY", process_query_command, NULL);
}
