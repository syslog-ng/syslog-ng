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

#include "commands.h"
#include "syslog-ng.h"

#include <unistd.h>

static gint attach_options_seconds = -1;
static gchar *attach_options_log_level = NULL;
static gint attach_options_fds_to_steel = 0;

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

static gboolean
_parse_fd_names(const gchar *option_name,
                const gchar *value,
                gpointer data,
                GError **error)
{
  gboolean result = TRUE;
  gchar **fds = g_strsplit(value, ",", 3);

  attach_options_fds_to_steel = 0;
  for (gchar **fd = fds; *fd; fd++)
    {
      if (g_str_equal(*fd, "stdin"))
        attach_options_fds_to_steel |= (1 << STDIN_FILENO);
      else if (g_str_equal(*fd, "stdout"))
        attach_options_fds_to_steel |= (1 << STDOUT_FILENO);
      else if (g_str_equal(*fd, "stderr"))
        attach_options_fds_to_steel |= (1 << STDERR_FILENO);
      else
        {
          g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED, "Unknown %s option value: %s", option_name, *fd);
          result = FALSE;
        }
    }
  g_strfreev(fds);
  return result;
}

gint
slng_attach(int argc, char *argv[], const gchar *mode, GOptionContext *ctx)
{
  GString *command = g_string_new("ATTACH");
  const gchar *attach_mode = mode ? : "stdio";

  if (g_str_equal(attach_mode, "stdio"))
    g_string_append(command, " STDIO");
  else if (g_str_equal(attach_mode, "logs"))
    g_string_append(command, " LOGS");
  else if (g_str_equal(attach_mode, "debugger"))
    g_string_append(command, " DEBUGGER");
  else
    {
      fprintf(stderr, "Error parsing command line arguments: Unknown attach mode\n");
      return 1;
    }
  if (FALSE == g_str_equal(attach_mode, "stdio") && attach_options_fds_to_steel > 0)
    {
      fprintf(stderr, "Error parsing command line arguments: %s is valid only with %s attach mode\n",
              "fds-to-steel", "stdio");
      return 1;
    }

  g_string_append_printf(command, " %d", attach_options_seconds > 0 ? attach_options_seconds : -1);
  g_string_append_printf(command, " %d",
                         attach_options_fds_to_steel > 0 ? attach_options_fds_to_steel : (1 << STDOUT_FILENO) | (1 << STDERR_FILENO));
  if (attach_options_log_level)
    g_string_append_printf(command, " %s", attach_options_log_level);

  gint result = attach_command(command->str);
  g_string_free(command, TRUE);
  return result;
}

#define SECONDS_OPTION_ENTRY OPTIONS_ENTRY("seconds", 's', 0, G_OPTION_ARG_INT, &attach_options_seconds, "amount of time to attach for", NULL)
#define LOG_LEVEL_OPTION_ENTRY OPTIONS_ENTRY("log-level", 'l', 0, G_OPTION_ARG_CALLBACK, _store_log_level, "change syslog-ng log level", "<default|verbose|debug|trace>")

const GOptionEntry attach_stdio_options[] =
{
  SECONDS_OPTION_ENTRY,
  LOG_LEVEL_OPTION_ENTRY,
  { "fds-to-steel", 'f', 0, G_OPTION_ARG_CALLBACK, _parse_fd_names, "which stdio file handlers to attach to, default is <stdout,stderr>", "<stdin|stdout|stderr> in a comma separated list"  },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
};

GOptionEntry attach_logs_and_debugger_options[] =
{
  SECONDS_OPTION_ENTRY,
  LOG_LEVEL_OPTION_ENTRY,
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
};

CommandDescriptor attach_commands[] =
{
  { "stdio", attach_stdio_options, "Attach to syslog-ng console using the given file handlers", slng_attach },
  { "logs", attach_logs_and_debugger_options, "Attach to syslog-ng internal logs, which are normally redirected via stderr; for further processing, use another redirection, e.g., `syslog-ng-ctl attach logs |& grep -i error.`", slng_attach },
  { "debugger", attach_logs_and_debugger_options, "Start and attach to syslog-ng debugger", slng_attach },
  { NULL, NULL, NULL, NULL }
};
