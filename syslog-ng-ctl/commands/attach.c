/*
 * Copyright (c) 2024 Balazs Scheidler <balazs.scheidler@axoflow.com>
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

#include "ctl-stats.h"
#include "syslog-ng.h"

static gint attach_options_seconds;
static gchar *attach_options_log_level = NULL;
static gchar **attach_commands = NULL;

static gboolean
_store_log_level(const gchar *option_name,
                 const gchar *value,
                 gpointer data,
                 GError **error)
{
  if (!attach_options_log_level)
    {
      attach_options_log_level = g_strdup(value);
      return TRUE;
    }
  g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED, "You can't specify multiple log-levels at a time.");
  return FALSE;
}

GOptionEntry attach_options[] =
{
  { "seconds", 0, 0, G_OPTION_ARG_INT, &attach_options_seconds, "amount of time to attach for", NULL },
  { "log-level", 0, 0, G_OPTION_ARG_CALLBACK, _store_log_level, "change syslog-ng log level", "<default|verbose|debug|trace>" },
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &attach_commands, "attach mode: logs, stdio", NULL },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
};

gint
slng_attach(int argc, char *argv[], const gchar *mode, GOptionContext *ctx)
{
  GString *command = g_string_new("ATTACH");
  const gchar *attach_mode;

  if (attach_commands)
    {
      if (attach_commands[1])
        {
          fprintf(stderr, "Too many arguments");
          return 1;
        }
      attach_mode = attach_commands[0];
    }
  else
    attach_mode = "stdio";

  if (g_str_equal(attach_mode, "stdio"))
    g_string_append(command, " STDIO");
  else if (g_str_equal(attach_mode, "logs"))
    g_string_append(command, " LOGS");
  else
    {
      fprintf(stderr, "Unknown attach mode\n");
      return 1;
    }

  g_string_append_printf(command, " %d", attach_options_seconds ? : -1);
  if (attach_options_log_level)
    g_string_append_printf(command, " %s", attach_options_log_level);
  gint result = attach_command(command->str);
  g_string_free(command, TRUE);
  return result;
}
