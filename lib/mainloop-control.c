/*
 * Copyright (c) 2002-2018 Balabit
 * Copyright (c) 1998-2018 Balázs Scheidler
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
#include "mainloop.h"
#include "control/control-commands.h"
#include "control/control-server.h"
#include "messages.h"
#include "cfg.h"
#include "apphook.h"
#include "secret-storage/secret-storage.h"

#include <string.h>

static GString *
control_connection_message_log(ControlConnection *cc, GString *command, gpointer user_data)
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
        }
      g_string_printf(result, "OK %s=%d", cmds[1], *type);
    }
  else
    g_string_assign(result, "FAIL Invalid arguments received");
exit:
  g_strfreev(cmds);
  return result;
}

static GString *
control_connection_stop_process(ControlConnection *cc, GString *command, gpointer user_data)
{
  GString *result = g_string_new("OK Shutdown initiated");
  MainLoop *main_loop = (MainLoop *) user_data;

  main_loop_exit(main_loop);
  return result;
}

static GString *
control_connection_config(ControlConnection *cc, GString *command, gpointer user_data)
{
  MainLoop *main_loop = (MainLoop *) user_data;
  GlobalConfig *config = main_loop_get_current_config(main_loop);
  GString *result = g_string_sized_new(128);
  gchar **arguments = g_strsplit(command->str, " ", 0);

  if (g_str_equal(arguments[1], "GET"))
    {
      if (g_str_equal(arguments[2], "ORIGINAL"))
        {
          g_string_assign(result, config->original_config->str);
          goto exit;
        }
      else if (g_str_equal(arguments[2], "PREPROCESSED"))
        {
          g_string_assign(result, config->preprocess_config->str);
          goto exit;
        }
    }

  if (g_str_equal(arguments[1], "VERIFY"))
    {
      main_loop_verify_config(result, main_loop);
      goto exit;
    }

  g_string_assign(result, "FAIL Invalid arguments received");

exit:
  g_strfreev(arguments);
  return result;
}

static GString *
show_ose_license_info(ControlConnection *cc, GString *command, gpointer user_data)
{
  return g_string_new("OK You are using the Open Source Edition of syslog-ng.");
}

static void
_respond_config_reload_status(gint type, gpointer user_data)
{
  gpointer *args = user_data;
  MainLoop *main_loop = (MainLoop *) args[0];
  ControlConnection *cc = (ControlConnection *) args[1];
  GString *reply;

  if (main_loop_was_last_reload_successful(main_loop))
    reply = g_string_new("OK Config reload successful");
  else
    reply = g_string_new("FAIL Config reload failed, reverted to previous config");

  control_connection_send_reply(cc, reply);
}

static GString *
control_connection_reload(ControlConnection *cc, GString *command, gpointer user_data)
{
  MainLoop *main_loop = (MainLoop *) user_data;
  static gpointer args[2];
  GError *error = NULL;


  if (!main_loop_reload_config_prepare(main_loop, &error))
    {
      GString *result = g_string_new("");
      g_string_printf(result, "FAIL %s, previous config remained intact", error->message);
      g_clear_error(&error);
      return result;
    }

  args[0] = main_loop;
  args[1] = cc;
  register_application_hook(AH_CONFIG_CHANGED, _respond_config_reload_status, args);
  main_loop_reload_config_commence(main_loop);
  return NULL;
}

static GString *
control_connection_reopen(ControlConnection *cc, GString *command, gpointer user_data)
{
  GString *result = g_string_new("OK Re-open of log destination files initiated");
  app_reopen_files();
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
secret_storage_status_accumulator(SecretStatus *status, gpointer user_data)
{
  GString *status_str = (GString *) user_data;
  g_string_append_printf(status_str, "%s\t%s\n", status->key, secret_status_to_string(status->state));
  return TRUE;
}

static GString *
process_credentials_status(GString *result)
{
  g_string_assign(result, "OK Credential storage status follows\n");
  secret_storage_status_foreach(secret_storage_status_accumulator, (gpointer) result);
  return result;
}

static GString *
process_credentials_add(GString *result, guint argc, gchar **arguments)
{
  if (argc < 4)
    {
      g_string_assign(result, "FAIL missing arguments to add\n");
      return result;
    }

  gchar *id = arguments[2];
  gchar *secret = arguments[3];

  if (secret_storage_store_secret(id, secret, strlen(secret)))
    g_string_assign(result, "OK Credentials stored successfully\n");
  else
    g_string_assign(result, "FAIL Error while saving credentials\n");

  secret_storage_wipe(secret, strlen(secret));
  return result;
}

static GString *
process_credentials(ControlConnection *cc, GString *command, gpointer user_data)
{
  gchar **arguments = g_strsplit(command->str, " ", 4);
  guint argc = g_strv_length(arguments);

  GString *result = g_string_new(NULL);

  if (argc < 1)
    {
      g_string_assign(result, "FAIL missing subcommand\n");
      g_strfreev(arguments);
      return result;
    }

  gchar *subcommand = arguments[1];

  if (strcmp(subcommand, "status") == 0)
    result = process_credentials_status(result);
  else if (g_strcmp0(subcommand, "add") == 0)
    result = process_credentials_add(result, argc, arguments);
  else
    g_string_printf(result, "FAIL invalid subcommand %s\n", subcommand);

  g_strfreev(arguments);
  return result;
}


ControlCommand default_commands[] =
{
  { "LOG", NULL, control_connection_message_log },
  { "STOP", NULL, control_connection_stop_process },
  { "RELOAD", NULL, control_connection_reload },
  { "REOPEN", NULL, control_connection_reopen },
  { "CONFIG", NULL, control_connection_config },
  { "LICENSE", NULL, show_ose_license_info },
  { "PWD", NULL, process_credentials },
  { NULL, NULL, NULL },
};

void
main_loop_register_control_commands(MainLoop *main_loop)
{
  int i;
  ControlCommand *cmd;

  for (i = 0; default_commands[i].command_name != NULL; i++)
    {
      cmd = &default_commands[i];
      control_register_command(cmd->command_name, cmd->description, cmd->func, main_loop);
    }
}
