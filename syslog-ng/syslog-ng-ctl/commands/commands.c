/*
 * Copyright (c) 2019 Balabit
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

#include "commands.h"
#include "control-client.h"

#include <string.h>
#include <stdio.h>

static ControlClient *control_client;

GOptionEntry no_options[] =
{
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

gboolean
is_syslog_ng_running(void)
{
  return control_client_connect(control_client);
}

gint
process_response_status(GString *response)
{
  if (strncmp(response->str, "FAIL ", 5) == 0)
    {
      g_string_erase(response, 0, 5);
      return 1;
    }
  else if (strncmp(response->str, "OK ", 3) == 0)
    {
      g_string_erase(response, 0, 3);
      return 0;
    }
  return 0;
}

static gboolean
slng_send_cmd(const gchar *cmd)
{
  if (!control_client_connect(control_client))
    {
      return FALSE;
    }

  if (control_client_send_command(control_client, cmd) < 0)
    {
      return FALSE;
    }

  return TRUE;
}

GString *
slng_run_command(const gchar *command)
{
  if (!slng_send_cmd(command))
    return NULL;

  return control_client_read_reply(control_client);
}

static gboolean
_is_response_empty(GString *response)
{
  return (response == NULL || g_str_equal(response->str, ""));
}

static void
clear_and_free(GString *str)
{
  if (str)
    {
      memset(str->str, 0, str->len);
      g_string_free(str, TRUE);
    }
}

gint
dispatch_command(const gchar *cmd)
{
  gint retval = 0;
  gchar *dispatchable_command = g_strdup_printf("%s\n", cmd);
  GString *rsp = slng_run_command(dispatchable_command);

  if (_is_response_empty(rsp))
    {
      retval = 1;
    }
  else
    {
      retval = process_response_status(rsp);
      printf("%s\n", rsp->str);
    }

  clear_and_free(rsp);

  secret_storage_wipe(dispatchable_command, strlen(dispatchable_command));
  g_free(dispatchable_command);

  return retval;
}

gint
run(const gchar *control_name, gint argc, gchar **argv, CommandDescriptor *mode, GOptionContext *ctx)
{
  control_client = control_client_new(control_name);
  gint result = mode->main(argc, argv, mode->mode, ctx);
  control_client_free(control_client);
  return result;
}
