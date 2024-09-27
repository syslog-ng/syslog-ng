/*
 * Copyright (c) 2024 Balabit
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
#include "internal-logs.h"
#include "commands.h"

static gboolean config_options_start = FALSE;
static gboolean config_options_stop = FALSE;
static gboolean config_options_size = FALSE;

GOptionEntry internal_options[] =
{
  { "start", 's', 0, G_OPTION_ARG_NONE, &config_options_start, "start live collection of internal logs", NULL },
  { "stop", 'x', 0, G_OPTION_ARG_NONE, &config_options_stop, "stop live collection of internal logs", NULL },
  { "size", 'l', 0, G_OPTION_ARG_NONE, &config_options_size, "get size of internal logs", NULL },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
};

gint
slng_internal_logs(int argc, char *argv[], const gchar *mode, GOptionContext *ctx)
{
  GString *cmd = g_string_new("");

  if (config_options_start)
    g_string_assign(cmd, "INTERLOGS START");
  else if (config_options_stop)
    g_string_assign(cmd, "INTERLOGS STOP");
  else if (config_options_size)
    g_string_assign(cmd, "INTERLOGS SIZE");
  else
    {
      fprintf(stderr, "Error: Unknown command\n");
      return 1;
    }

  gint res = dispatch_command(cmd->str);
  g_string_free(cmd, TRUE);

  return res;
}
