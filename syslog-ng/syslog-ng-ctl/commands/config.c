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

#include "config.h"
#include "commands.h"

static gboolean config_options_preprocessed = FALSE;
static gboolean config_options_verify = FALSE;

GOptionEntry config_options[] =
{
  { "preprocessed", 'p', 0, G_OPTION_ARG_NONE, &config_options_preprocessed, "preprocessed", NULL },
  { "verify", 'v', 0, G_OPTION_ARG_NONE, &config_options_verify, "verify", NULL },
  { NULL,           0,   0, G_OPTION_ARG_NONE, NULL,                         NULL,           NULL }
};

gint
slng_config(int argc, char *argv[], const gchar *mode, GOptionContext *ctx)
{
  GString *cmd = g_string_new("CONFIG ");

  if (config_options_verify)
    g_string_append(cmd, "VERIFY ");
  else
    {
      g_string_append(cmd, "GET ");

      if(config_options_preprocessed)
        g_string_append(cmd, "PREPROCESSED");
      else
        g_string_append(cmd, "ORIGINAL");
    }

  gint res = dispatch_command(cmd->str);
  g_string_free(cmd, TRUE);

  return res;
}
