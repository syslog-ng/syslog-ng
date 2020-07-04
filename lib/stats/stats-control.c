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
#include "stats/stats-query-commands.h"
#include "control/control-commands.h"
#include "control/control-server.h"

#include <iv.h>
#include <iv_event.h>

typedef GString *(*ControlConnectionCommand)(ControlConnection *cc, GString *command, gpointer user_data);

typedef struct _ThreadedCommandRunner
{
  ControlConnection *connection;
  GString *command;
  gpointer user_data;

  GThread *thread;
  ControlConnectionCommand func;
  GString *response;
  struct iv_event response_received;
} ThreadedCommandRunner;

static ThreadedCommandRunner *
_thread_command_runner_new(ControlConnection *cc, GString *cmd, gpointer user_data)
{
  ThreadedCommandRunner *self = g_new0(ThreadedCommandRunner, 1);
  self->connection = cc;
  self->command = cmd;
  self->user_data = user_data;

  return self;
}

static void
_send_response(gpointer user_data)
{
  ThreadedCommandRunner *self = (ThreadedCommandRunner *) user_data;
  g_thread_join(self->thread);
  control_connection_send_reply(self->connection, self->response);
  iv_event_unregister(&self->response_received);
  g_free(self);
}

static void
_thread(gpointer user_data)
{
  ThreadedCommandRunner *self = (ThreadedCommandRunner *)user_data;
  self->response = self->func(self->connection, self->command, self->user_data);
  iv_event_post(&self->response_received);
}

static void
_thread_command_runner_run(ThreadedCommandRunner *self, ControlConnectionCommand func)
{
  IV_EVENT_INIT(&self->response_received);
  self->response_received.handler = _send_response;
  self->response_received.cookie = self;
  iv_event_register(&self->response_received);
  self->func = func;
  self->thread = g_thread_new(self->command->str, (GThreadFunc) _thread, self);
}

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
  ThreadedCommandRunner *runner = _thread_command_runner_new(cc, command, user_data);
  _thread_command_runner_run(runner, _send_stats_get_result);
}

static void
control_connection_reset_stats(ControlConnection *cc, GString *command, gpointer user_data)
{
  GString *result = g_string_new("OK The statistics of syslog-ng have been reset to 0.");
  _reset_counters();
  control_connection_send_reply(cc, result);
}

void
stats_register_control_commands(void)
{
  control_register_command("STATS", control_connection_send_stats, NULL);
  control_register_command("RESET_STATS", control_connection_reset_stats, NULL);
  control_register_command("QUERY", process_query_command, NULL);
}
