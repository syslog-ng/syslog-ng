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
#include "messages.h"

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
control_register_command(const gchar *command_name, CommandFunction function,
                         gpointer user_data)
{
  GList *command_it = g_list_find_custom(command_list, command_name, (GCompareFunc)control_command_start_with_command);
  if (command_it)
    {
      ControlCommand *cmd = (ControlCommand *)command_it->data;
      if (cmd->func != function)
        {
          msg_debug("Trying to register an already registered ControlCommand with different CommandFunction.");
        }
      return;
    }
  ControlCommand *new_command = g_new0(ControlCommand, 1);
  new_command->command_name = command_name;
  new_command->func = function;
  new_command->user_data = user_data;
  command_list = g_list_append(command_list, new_command);
}

void
control_replace_command(const gchar *command_name, CommandFunction function,
                        gpointer user_data)
{
  GList *command_it =  g_list_find_custom(command_list, command_name,
                                          (GCompareFunc)control_command_start_with_command);
  if (!command_it)
    {
      msg_debug("Trying to replace a non-existent command. Command will be registered as a new command.",
                evt_tag_str("command", command_name));
      control_register_command(command_name, function, user_data);
      return;
    }

  ControlCommand *command = (ControlCommand *)command_it->data;
  command->func = function;
  command->user_data = user_data;
}
