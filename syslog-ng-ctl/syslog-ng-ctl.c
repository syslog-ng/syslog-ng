/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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

#include "syslog-ng.h"
#include "gsocket.h"
#include "control-client.h"
#include "cfg.h"
#include "reloc.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if SYSLOG_NG_HAVE_GETOPT_H
#include <getopt.h>
#endif

static const gchar *control_name;
static ControlClient *control_client;

static gboolean
slng_send_cmd(const gchar *cmd)
{
  if (!control_client_connect(control_client))
    {
      return FALSE;
    }

  if (control_client_send_command(control_client,cmd) < 0)
    {
      return FALSE;
    }

  return TRUE;
}

static GString *
slng_run_command(const gchar *command)
{
  if (!slng_send_cmd(command))
    return NULL;

  return control_client_read_reply(control_client);
}

static gchar *verbose_set = NULL;

static gint
slng_verbose(int argc, char *argv[], const gchar *mode)
{
  gint ret = 0;
  GString *rsp = NULL;
  gchar buff[256];

  if (!verbose_set)
    snprintf(buff, 255, "LOG %s\n", mode);
  else
    snprintf(buff, 255, "LOG %s %s\n", mode,
        strncasecmp(verbose_set, "on", 2) == 0 || verbose_set[0] == '1' ? "ON" : "OFF");

  g_strup(buff);

  rsp = slng_run_command(buff);
  if (rsp == NULL)
    return 1;

  if (!verbose_set)
    printf("%s\n", rsp->str);
  else
    ret = g_str_equal(rsp->str, "OK");

  g_string_free(rsp, TRUE);

  return ret;
}

static gboolean stats_options_reset_is_set = FALSE;

static GOptionEntry stats_options[] =
{
  { "reset", 'r', 0, G_OPTION_ARG_NONE, &stats_options_reset_is_set, "reset counters", NULL },
  { NULL,    0,   0, G_OPTION_ARG_NONE, NULL,                        NULL,             NULL }
};

static GOptionEntry verbose_options[] =
{
  { "set", 's', 0, G_OPTION_ARG_STRING, &verbose_set,
    "enable/disable messages", "<on|off|0|1>" },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};


static const gchar *
_stats_command_builder()
{
  return stats_options_reset_is_set ? "RESET_STATS\n" : "STATS\n";
}

static gint
slng_stats(int argc, char *argv[], const gchar *mode)
{
  GString *rsp = slng_run_command(_stats_command_builder());

  if (rsp == NULL)
    return 1;

  printf("%s\n", rsp->str);

  g_string_free(rsp, TRUE);

  return 0;
}

static gint
slng_stop(int argc, char *argv[], const gchar *mode)
{
  GString *rsp = slng_run_command("STOP\n");

  if (rsp == NULL)
    return 1;

  printf("%s\n", rsp->str);

  g_string_free(rsp, TRUE);

  return 0;
}

static gint
slng_reload(int argc, char *argv[], const gchar *mode)
{
  GString *rsp = slng_run_command("RELOAD\n");

  if (rsp == NULL)
    return 1;

  printf("%s\n", rsp->str);

  g_string_free(rsp, TRUE);

  return 0;
}

static GOptionEntry no_options[] =
{
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

const gchar *
get_mode(int *argc, char **argv[])
{
  gint i;
  const gchar *mode;

  for (i = 1; i < (*argc); i++)
    {
      if ((*argv)[i][0] != '-')
        {
          mode = (*argv)[i];
          memmove(&(*argv)[i], &(*argv)[i+1], ((*argc) - i) * sizeof(gchar *));
          (*argc)--;
          return mode;
        }
    }
  return NULL;
}

static GOptionEntry slng_options[] =
{
  { "control", 'c', 0, G_OPTION_ARG_STRING, &control_name,
    "syslog-ng control socket", "<socket>" },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

static struct
{
  const gchar *mode;
  const GOptionEntry *options;
  const gchar *description;
  gint (*main)(gint argc, gchar *argv[], const gchar *mode);
} modes[] =
{
  { "stats", stats_options, "Query/reset syslog-ng statistics", slng_stats },
  { "verbose", verbose_options, "Enable/query verbose messages", slng_verbose },
  { "debug", verbose_options, "Enable/query debug messages", slng_verbose },
  { "trace", verbose_options, "Enable/query trace messages", slng_verbose },
  { "stop", no_options, "Stop syslog-ng process", slng_stop },
  { "reload", no_options, "Reload syslog-ng", slng_reload },
  { NULL, NULL },
};

void
usage(const gchar *bin_name)
{
  gint mode;

  fprintf(stderr, "Syntax: %s <command> [options]\nPossible commands are:\n", bin_name);
  for (mode = 0; modes[mode].mode; mode++)
    {
      fprintf(stderr, "    %-12s %s\n", modes[mode].mode, modes[mode].description);
    }
  exit(1);
}

int
main(int argc, char *argv[])
{
  const gchar *mode_string;
  GOptionContext *ctx;
  gint mode;
  GError *error = NULL;
  int result;

  control_name = get_installation_path_for(PATH_CONTROL_SOCKET);

  mode_string = get_mode(&argc, &argv);
  if (!mode_string)
    {
      usage(argv[0]);
    }

  ctx = NULL;
  for (mode = 0; modes[mode].mode; mode++)
    {
      if (strcmp(modes[mode].mode, mode_string) == 0)
        {
          ctx = g_option_context_new(mode_string);
          #if GLIB_CHECK_VERSION (2, 12, 0)
          g_option_context_set_summary(ctx, modes[mode].description);
          #endif
          g_option_context_add_main_entries(ctx, modes[mode].options, NULL);
          g_option_context_add_main_entries(ctx, slng_options, NULL);
          break;
        }
    }
  if (!ctx)
    {
      fprintf(stderr, "Unknown command\n");
      usage(argv[0]);
    }

  if (!g_option_context_parse(ctx, &argc, &argv, &error))
    {
      fprintf(stderr, "Error parsing command line arguments: %s\n", error ? error->message : "Invalid arguments");
      g_clear_error(&error);
      g_option_context_free(ctx);
      return 1;
    }
  g_option_context_free(ctx);

  control_client = control_client_new(control_name);

  result = modes[mode].main(argc, argv, modes[mode].mode);
  control_client_free(control_client);
  return result;
}
