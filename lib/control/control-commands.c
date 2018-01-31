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
#include "apphook.h"
#include "stats/stats-query-commands.h"

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


static gboolean
_reload(GString *command, gpointer user_data, GString **result)
{
  MainLoop *main_loop = (MainLoop *) user_data;
  gboolean reload_initiated = main_loop_reload_config(main_loop);

  if (reload_initiated)
    {
      *result = g_string_new("OK Config reload initiated");
    }
  else
    {
      *result = g_string_new("FAIL Failed to request config reload");
    }

  return reload_initiated;
}

static GString *
control_connection_reload(GString *command, gpointer user_data)
{
  GString *result = NULL;
  _reload(command, user_data, &result);
  return result;
}

typedef struct _ReloadCondvar
{
  GMutex mutex;
  GCond condvar;
  gboolean reloaded;
} ReloadCondvar;

static ReloadCondvar *
_reload_condvar_new()
{
  ReloadCondvar *self = g_new0(ReloadCondvar, 1);
  g_mutex_init(&self->mutex);
  g_cond_init(&self->condvar);
  self->reloaded = FALSE;

  return self;
}

static void
_reload_condvar_free(ReloadCondvar *self)
{
  g_mutex_clear(&self->mutex);
  g_cond_clear(&self->condvar);
  g_free(self);
}

static void
_reload_condvar_emit(ReloadCondvar *self)
{
  g_mutex_lock(&self->mutex);
  self->reloaded = TRUE;
  g_cond_signal(&self->condvar);
  g_mutex_unlock(&self->mutex);
}

static void
_reload_condvar_wait(ReloadCondvar *self)
{
  g_mutex_lock(&self->mutex);
  while (!self->reloaded)
    g_cond_wait(&self->condvar, &self->mutex);
  self->reloaded = FALSE;
  g_mutex_unlock(&self->mutex);
}

static void
_config_reloaded(gint hook_type, gpointer user_data)
{
  ReloadCondvar *self = (ReloadCondvar *)user_data;
  _reload_condvar_emit(self);
}

static void
_wait_for_syslog_ng_reload()
{
  ReloadCondvar *reload_condvar = _reload_condvar_new();
  register_application_hook_without_checking_current_state(AH_POST_CONFIG_LOADED, _config_reloaded, reload_condvar);
  _reload_condvar_wait(reload_condvar);
  _reload_condvar_free(reload_condvar);
}

static GString *
control_connection_reload_sync(GString *command, gpointer user_data)
{
  GString *result = NULL;
  gboolean success = _reload(command, user_data, &result);

  if (!success)
    return result;

  _wait_for_syslog_ng_reload();

  return result;
}

static GString *
control_connection_reopen(GString *command, gpointer user_data)
{
  GString *result = g_string_new("OK Re-open of log destination files initiated");
  app_reopen();
  return result;
}

ControlCommand default_commands[] =
{
  { "LOG", NULL, control_connection_message_log },
  { "STOP", NULL, control_connection_stop_process },
  { "RELOAD", NULL, control_connection_reload },
  { "REOPEN", NULL, control_connection_reopen },
  { "QUERY", NULL, process_query_command },
  { "SYNCRELOAD", NULL, control_connection_reload_sync },
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
