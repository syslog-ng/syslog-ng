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
#include "control/control.h"
#include "control/control-main.h"
#include "mainloop.h"
#include "messages.h"
#include "stats/stats-query-commands.h"

static GList *command_list = NULL;

GList *
get_control_command_list()
{
  return command_list;
}

void
reset_control_command_list()
{
  g_list_free_full(command_list, (GDestroyNotify)g_free);
  command_list = NULL;
}

void
control_register_command(const gchar *command_name, const gchar *description, CommandFunction function,
                         gpointer user_data)
{
  ControlCommand *new_command = g_new0(ControlCommand, 1);
  new_command->command_name = command_name;
  new_command->description = description;
  new_command->func = function;
  new_command->user_data = user_data;
  command_list = g_list_append(command_list, new_command);
}

static GString *
control_connection_message_log(GString *command, gpointer user_data)
{
  gchar **cmds = g_strsplit(command->str, " ", 3);
  gboolean on;
  int *type = NULL;
  GString *result = g_string_sized_new(128);

  if (!cmds[1])
    {
      g_string_assign(result,"Invalid arguments received, expected at least one argument");
      goto exit;
    }

  if (g_str_equal(cmds[1], "DEBUG"))
    type = &debug_flag;
  else if (g_str_equal(cmds[1], "VERBOSE"))
    type = &verbose_flag;
  else if (g_str_equal(cmds[1], "TRACE"))
    type = &trace_flag;

  if (type)
    {
      if (cmds[2])
        {
          on = g_str_equal(cmds[2], "ON");
          if (*type != on)
            {
              msg_info("Verbosity changed", evt_tag_str("type", cmds[1]), evt_tag_int("on", on));
              *type = on;
            }

          g_string_assign(result,"OK");
        }
      else
        {
          g_string_printf(result,"%s=%d", cmds[1], *type);
        }
    }
  else
    g_string_assign(result, "Invalid arguments received");
exit:
  g_strfreev(cmds);
  return result;
}

static GString *
control_connection_stop_process(GString *command, gpointer user_data)
{
  GString *result = g_string_new("OK Shutdown initiated");
  MainLoop *main_loop = (MainLoop *) user_data;

  main_loop_exit(main_loop);
  return result;
}

static GString *
control_connection_reload(GString *command, gpointer user_data)
{
  GString *result = g_string_new("OK Config reload initiated");
  MainLoop *main_loop = (MainLoop *) user_data;

  main_loop_reload_config(main_loop);
  return result;
}

ControlCommand default_commands[] =
{
  { "LOG", NULL, control_connection_message_log },
  { "STOP", NULL, control_connection_stop_process },
  { "RELOAD", NULL, control_connection_reload },
  { "QUERY", NULL, process_query_command },
  { NULL, NULL, NULL },
};

GList *
control_register_default_commands(MainLoop *main_loop)
{
  int i;
  ControlCommand *cmd;

  for (i = 0; default_commands[i].command_name != NULL; i++)
    {
      cmd = &default_commands[i];
      control_register_command(cmd->command_name, cmd->description, cmd->func, main_loop);
    }
  return command_list;
}
