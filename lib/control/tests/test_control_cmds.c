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

#include <criterion/criterion.h>

#include "messages.h"
#include "control/control.h"
#include "control/control-commands.h"
#include "mainloop-control.h"
#include "stats/stats-control.h"
#include "control/control-server.h"
#include "stats/stats-cluster.h"
#include "stats/stats-registry.h"
#include "apphook.h"

static ControlConnection dummy = {0};
static ControlConnection *conn = &dummy;

static ControlCommand *
command_test_get(const char *cmd)
{
  GList *command = g_list_find_custom(get_control_command_list(), cmd, (GCompareFunc)control_command_start_with_command);
  if (NULL == command)
    return NULL;
  return (ControlCommand *)command->data;
}

void
setup(void)
{
  msg_init(FALSE);
  main_loop_register_control_commands(NULL);
  stats_register_control_commands();
}

void
teardown(void)
{
  msg_deinit();
  reset_control_command_list();
}

TestSuite(control_cmds, .init = setup, .fini = teardown);

Test(control_cmds, test_log)
{
  GString *command = g_string_sized_new(128);
  GString *reply;

  CommandFunction control_connection_message_log = command_test_get("LOG")->func;

  g_string_assign(command,"LOG");
  reply = control_connection_message_log(conn, command, NULL);
  cr_assert_str_eq(reply->str, "Invalid arguments received, expected at least one argument", "Bad reply");
  g_string_free(reply, TRUE);

  g_string_assign(command,"LOG fakelog");
  reply = control_connection_message_log(conn, command, NULL);
  cr_assert_str_eq(reply->str, "Invalid arguments received", "Bad reply");
  g_string_free(reply, TRUE);

  verbose_flag = 0;
  debug_flag = 1;
  trace_flag = 1;
  g_string_assign(command,"LOG VERBOSE");
  reply = control_connection_message_log(conn, command, NULL);
  cr_assert_str_eq(reply->str, "VERBOSE=0", "Bad reply");
  g_string_free(reply, TRUE);

  g_string_assign(command,"LOG VERBOSE ON");
  reply = control_connection_message_log(conn, command, NULL);
  cr_assert_str_eq(reply->str, "OK", "Bad reply");
  cr_assert_eq(verbose_flag,1,"Flag isn't changed");
  g_string_free(reply, TRUE);

  g_string_assign(command,"LOG VERBOSE OFF");
  reply = control_connection_message_log(conn, command, NULL);
  cr_assert_str_eq(reply->str, "OK", "Bad reply");
  cr_assert_eq(verbose_flag,0,"Flag isn't changed");
  g_string_free(reply, TRUE);

  debug_flag = 0;
  verbose_flag = 1;
  trace_flag = 1;
  g_string_assign(command,"LOG DEBUG");
  reply = control_connection_message_log(conn, command, NULL);
  cr_assert_str_eq(reply->str, "DEBUG=0", "Bad reply");
  g_string_free(reply, TRUE);

  trace_flag = 0;
  verbose_flag = 1;
  debug_flag = 1;
  g_string_assign(command,"LOG TRACE");
  reply = control_connection_message_log(conn, command, NULL);
  cr_assert_str_eq(reply->str, "TRACE=0", "Bad reply");
  g_string_free(reply, TRUE);

  g_string_free(command, TRUE);
}

Test(control_cmds, test_stats)
{
  GString *reply = NULL;
  GString *command = g_string_sized_new(128);
  StatsCounterItem *counter = NULL;
  gchar **stats_result;

  stats_init();

  CommandFunction control_connection_send_stats = command_test_get("STATS")->func;

  stats_lock();
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key, SCS_CENTER, "id", "received" );
  stats_register_counter(0, &sc_key, SC_TYPE_PROCESSED, &counter);
  stats_unlock();

  g_string_assign(command,"STATS");

  reply = control_connection_send_stats(conn, command, NULL);
  stats_result = g_strsplit(reply->str, "\n", 2);
  cr_assert_str_eq(stats_result[0], "SourceName;SourceId;SourceInstance;State;Type;Number",
                   "Bad reply");
  g_strfreev(stats_result);
  g_string_free(reply, TRUE);

  g_string_free(command, TRUE);
  stats_destroy();
}

Test(control_cmds, test_reset_stats)
{
  GString *reply = NULL;
  GString *command = g_string_sized_new(128);
  StatsCounterItem *counter = NULL;

  stats_init();

  CommandFunction control_connection_send_stats = command_test_get("STATS")->func;
  CommandFunction control_connection_reset_stats = command_test_get("RESET_STATS")->func;

  stats_lock();
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key, SCS_CENTER, "id", "received" );
  stats_register_counter(0, &sc_key, SC_TYPE_PROCESSED, &counter);
  stats_counter_set(counter, 666);
  stats_unlock();

  g_string_assign(command, "RESET_STATS");
  reply = control_connection_reset_stats(conn, command, NULL);
  cr_assert_str_eq(reply->str, "The statistics of syslog-ng have been reset to 0.", "Bad reply");
  g_string_free(reply, TRUE);

  reply = control_connection_send_stats(conn, command, NULL);
  cr_assert_str_eq(reply->str, "SourceName;SourceId;SourceInstance;State;Type;Number\ncenter;id;received;a;processed;0\n",
                   "Bad reply");
  g_string_free(reply, TRUE);

  stats_destroy();
  g_string_free(command, TRUE);
}

static GString *
_original_replace(ControlConnection *cc, GString *result, gpointer user_data)
{
  return NULL;
}

static GString *
_new_replace(ControlConnection *cc, GString *result, gpointer user_data)
{
  return NULL;
}

static void
_assert_control_command_eq(ControlCommand *cmd, ControlCommand *cmd_other)
{
  cr_assert_eq(cmd->func, cmd_other->func);
  cr_assert_str_eq(cmd->command_name, cmd_other->command_name);
  cr_assert_str_eq(cmd->description, cmd_other->description);
  cr_assert_eq(cmd->user_data, cmd_other->user_data);
}

Test(control_cmds, test_replace_existing_command)
{
  control_register_command("REPLACE", "original", _original_replace, (gpointer)0xbaadf00d);
  ControlCommand *cmd = command_test_get("REPLACE");
  ControlCommand expected_original =
  {
    .func = _original_replace,
    .command_name = "REPLACE",
    .description = "original",
    .user_data = (gpointer)0xbaadf00d
  };

  _assert_control_command_eq(cmd, &expected_original);

  control_replace_command("REPLACE", "new", _new_replace, (gpointer)0xd006f00d);
  ControlCommand *new_cmd = command_test_get("REPLACE");
  ControlCommand expected_new =
  {
    .func = _new_replace,
    .command_name = "REPLACE",
    .description = "new",
    .user_data = (gpointer) 0xd006f00d
  };
  _assert_control_command_eq(new_cmd, &expected_new);
}

Test(control_cmds, test_replace_non_existing_command)
{
  control_replace_command("REPLACE", "new", _new_replace, (gpointer)0xd006f00d);
  ControlCommand *new_cmd = command_test_get("REPLACE");
  ControlCommand expected_new =
  {
    .func = _new_replace,
    .command_name = "REPLACE",
    .description = "new",
    .user_data = (gpointer) 0xd006f00d
  };
  _assert_control_command_eq(new_cmd, &expected_new);
}
