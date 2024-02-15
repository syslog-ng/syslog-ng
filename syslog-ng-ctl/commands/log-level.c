/*
 * Copyright (c) 2022 Balazs Scheidler <bazsi77@gmail.com>
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

#include "log-level.h"
#include <strings.h>

static gchar *log_level = NULL;

static gboolean
_store_log_level(const gchar *option_name,
                 const gchar *value,
                 gpointer data,
                 GError **error)
{
  if (!log_level)
    {
      log_level = g_strdup(value);
      return TRUE;
    }
  g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED, "You can't specify multiple log-levels at a time.");
  return FALSE;
}

GOptionEntry log_level_options[] =
{
  {
    G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_CALLBACK, _store_log_level,
    "Set log level", "<default|verbose|debug|trace>"
  },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

gint
slng_log_level(int argc, char *argv[], const gchar *mode, GOptionContext *ctx)
{
  gchar buf[256];

  if (log_level)
    g_snprintf(buf, sizeof(buf), "LOG LEVEL %s\n", log_level);
  else
    g_snprintf(buf, sizeof(buf), "LOG LEVEL\n");

  gchar *command = g_ascii_strup(buf, -1);

  gint ret = dispatch_command(command);

  g_free(command);
  g_free(log_level);
  return ret;
}
