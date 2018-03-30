/*
 * Copyright (c) 2013-2015 Balabit
 * Copyright (c) 2013 Juhász Viktor <jviktor@balabit.hu>
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


#include "testutils.h"
#include "lib/messages.h"
#include "control/control.h"
#include "lib/control/control-commands.h"
#include "stats/stats-control.h"
#include "control/control-server.h"
#include "stats/stats-cluster.h"
#include "stats/stats-registry.h"
#include "apphook.h"

static GList *command_list = NULL;

static ControlCommand *
command_test_get(const char *cmd)
{
  GList *command = g_list_find_custom(command_list, cmd, (GCompareFunc)control_command_start_with_command);
  if (NULL == command)
    return NULL;
  return (ControlCommand *)command->data;
}

void
test_log(void)
{
  GString *command = g_string_sized_new(128);
  GString *reply;

  CommandFunction control_connection_message_log = command_test_get("LOG")->func;

  g_string_assign(command,"LOG");
  reply = control_connection_message_log(command, NULL);
  assert_string(reply->str, "Invalid arguments received, expected at least one argument", "Bad reply");
  g_string_free(reply, TRUE);

  g_string_assign(command,"LOG fakelog");
  reply = control_connection_message_log(command, NULL);
  assert_string(reply->str, "Invalid arguments received", "Bad reply");
  g_string_free(reply, TRUE);

  verbose_flag = 0;
  debug_flag = 1;
  trace_flag = 1;
  g_string_assign(command,"LOG VERBOSE");
  reply = control_connection_message_log(command, NULL);
  assert_string(reply->str, "VERBOSE=0", "Bad reply");
  g_string_free(reply, TRUE);

  g_string_assign(command,"LOG VERBOSE ON");
  reply = control_connection_message_log(command, NULL);
  assert_string(reply->str, "OK", "Bad reply");
  assert_gint(verbose_flag,1,"Flag isn't changed");
  g_string_free(reply, TRUE);

  g_string_assign(command,"LOG VERBOSE OFF");
  reply = control_connection_message_log(command, NULL);
  assert_string(reply->str, "OK", "Bad reply");
  assert_gint(verbose_flag,0,"Flag isn't changed");
  g_string_free(reply, TRUE);

  debug_flag = 0;
  verbose_flag = 1;
  trace_flag = 1;
  g_string_assign(command,"LOG DEBUG");
  reply = control_connection_message_log(command, NULL);
  assert_string(reply->str, "DEBUG=0", "Bad reply");
  g_string_free(reply, TRUE);

  trace_flag = 0;
  verbose_flag = 1;
  debug_flag = 1;
  g_string_assign(command,"LOG TRACE");
  reply = control_connection_message_log(command, NULL);
  assert_string(reply->str, "TRACE=0", "Bad reply");
  g_string_free(reply, TRUE);

  g_string_free(command, TRUE);
  return;
}

void
test_stats(void)
{
  GString *reply = NULL;
  GString *command = g_string_sized_new(128);
  StatsCounterItem *counter = NULL;
  gchar **stats_result;

  stats_init();
  command_list = control_register_default_commands(NULL);

  CommandFunction control_connection_send_stats = command_test_get("STATS")->func;

  stats_lock();
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key, SCS_CENTER, "id", "received" );
  stats_register_counter(0, &sc_key, SC_TYPE_PROCESSED, &counter);
  stats_unlock();

  g_string_assign(command,"STATS");

  reply = control_connection_send_stats(command, NULL);
  stats_result = g_strsplit(reply->str, "\n", 2);
  assert_string(stats_result[0], "SourceName;SourceId;SourceInstance;State;Type;Number",
                "Bad reply");
  g_strfreev(stats_result);
  g_string_free(reply, TRUE);

  g_string_free(command, TRUE);
  stats_destroy();
  return;
}

void
test_reset_stats(void)
{
  GString *reply = NULL;
  GString *command = g_string_sized_new(128);
  StatsCounterItem *counter = NULL;

  stats_init();
  command_list = control_register_default_commands(NULL);

  CommandFunction control_connection_send_stats = command_test_get("STATS")->func;
  CommandFunction control_connection_reset_stats = command_test_get("RESET_STATS")->func;

  stats_lock();
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key, SCS_CENTER, "id", "received" );
  stats_register_counter(0, &sc_key, SC_TYPE_PROCESSED, &counter);
  stats_counter_set(counter, 666);
  stats_unlock();

  g_string_assign(command, "RESET_STATS");
  reply = (*control_connection_reset_stats)(command, NULL);
  assert_string(reply->str, "The statistics of syslog-ng have been reset to 0.", "Bad reply");
  g_string_free(reply, TRUE);

  g_string_assign(command, "STATS");
  reply = control_connection_send_stats(command, NULL);
  assert_string(reply->str, "SourceName;SourceId;SourceInstance;State;Type;Number\ncenter;id;received;a;processed;0\n",
                "Bad reply");
  g_string_free(reply, TRUE);

  stats_destroy();
  g_string_free(command, TRUE);
  return;
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  msg_init(FALSE);
  stats_register_control_commands();
  command_list = control_register_default_commands(NULL);
  test_log();
  test_stats();
  test_reset_stats();
  msg_deinit();
  reset_control_command_list();
  return 0;
}
