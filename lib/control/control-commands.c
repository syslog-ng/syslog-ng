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
#include "lib/secret-storage/secret-storage.h"
#include "mainloop.h"
#include "messages.h"
#include "apphook.h"
#include "stats/stats-query-commands.h"
#include "string.h"

static GList *command_list = NULL;

GList *
get_control_command_list(void)
{
  return command_list;
}

void
reset_control_command_list(void)
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

static GString *
control_connection_reopen(GString *command, gpointer user_data)
{
  GString *result = g_string_new("OK Re-open of log destination files initiated");
  app_reopen();
  return result;
}

static const gchar *
secret_status_to_string(SecretStorageSecretState state)
{
  switch (state)
    {
    case SECRET_STORAGE_STATUS_PENDING:
      return "PENDING";
    case SECRET_STORAGE_SUCCESS:
      return "SUCCESS";
    case SECRET_STORAGE_STATUS_FAILED:
      return "FAILED";
    case SECRET_STORAGE_STATUS_INVALID_PASSWORD:
      return "INVALID_PASSWORD";
    default:
      g_assert_not_reached();
    }
  return "SHOULD NOT BE REACHED";
}

gboolean
secret_storage_status_iterator(SecretStatus *status, gpointer user_data)
{
  GString *status_str = (GString *) user_data;
  g_string_append_printf(status_str, "%s\t%s\n", status->key, secret_status_to_string(status->state));
  return TRUE;
}

static GString *
process_credentials_status(GString *result)
{
  g_string_assign(result,"Credential storage status:\n");
  secret_storage_status_foreach(secret_storage_status_iterator, (gpointer) result);
  return result;
}

static GString *
process_credentials_add(GString *result, guint argc, gchar **arguments)
{
  if (argc < 4)
    {
      g_string_assign(result,"error: missing arguments\n");
      return result;
    }

  gchar *id = arguments[2];
  gchar *secret = arguments[3];

  if (secret_storage_store_string(id,secret))
    g_string_assign(result,"Credentials stored successfully\n");
  else
    g_string_assign(result,"Error while saving credentials\n");
  return result;
}

static GString *
process_credentials(GString *command, gpointer user_data)
{
  gchar **arguments = g_strsplit(command->str, " ", 4);
  guint argc = g_strv_length(arguments);

  GString *result = g_string_new(NULL);

  if (argc < 1)
    {
      g_string_assign(result,"error: missing subcommand\n");
      g_strfreev(arguments);
      return result;
    }

  gchar *subcommand = arguments[1];

  if (strcmp(arguments[1],"status")==0)
    result = process_credentials_status(result);
  else if (g_strcmp0(subcommand,"add") == 0)
    result = process_credentials_add(result, argc, arguments);
  else
    g_string_printf(result,"error: invalid subcommand %s\n", subcommand);

  g_strfreev(arguments);
  return result;
}

ControlCommand default_commands[] =
{
  { "LOG", NULL, control_connection_message_log },
  { "STOP", NULL, control_connection_stop_process },
  { "RELOAD", NULL, control_connection_reload },
  { "REOPEN", NULL, control_connection_reopen },
  { "QUERY", NULL, process_query_command },
  { "PWD", NULL, process_credentials },
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
