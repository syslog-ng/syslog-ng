/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if HAVE_GETOPT_H
#include <getopt.h>
#endif

static gchar *control_name = PATH_CONTROL_SOCKET;
static gint control_socket = -1;

static gboolean
slng_send_cmd(gchar *cmd)
{
  GSockAddr *saddr = NULL;

  if (control_socket == -1)
    {
      saddr = g_sockaddr_unix_new(control_name);
      control_socket = socket(PF_UNIX, SOCK_STREAM, 0);

      if (control_socket == -1)
        {
          fprintf(stderr, "Error opening control socket, socket='%s', error='%s'\n", control_name, strerror(errno));
          goto error;
        }

      if (g_connect(control_socket, saddr) != G_IO_STATUS_NORMAL)
        {
          fprintf(stderr, "Error connecting control socket, socket='%s', error='%s'\n", control_name, strerror(errno));
          close(control_socket);
          control_socket = -1;
          goto error;
        }
    }

  if (write(control_socket, cmd, strlen(cmd)) < 0)
    {
      fprintf(stderr, "Error sending command on control sokcet, socket='%s', cmd='%s', error='%s'\n", control_name, cmd, strerror(errno));
      goto error;
    }

  return TRUE;

error:
  if (saddr)
    g_sockaddr_unref(saddr);

  return FALSE;
}

#define BUFF_LEN 8192

static GString *
slng_read_response(void)
{
  gsize len = 0;
  gchar buff[BUFF_LEN];
  GString *rsp = NULL;

  if (control_socket == -1)
    {
      fprintf(stderr, "Error reading control socket, socket is not connected\n");
      return NULL;
    }

  rsp = g_string_sized_new(256);

  while (1)
    {
      if ((len = read(control_socket, buff, BUFF_LEN - 1)) < 0)
        {
          fprintf(stderr, "Error reading control socket, error='%s'\n", strerror(errno));
          g_string_free(rsp, TRUE);
          return NULL;
        }

      if (len == 0)
        {
          fprintf(stderr, "EOF occured while reading control socket\n");
          g_string_free(rsp, TRUE);
          return NULL;
        }

      g_string_append_len(rsp, buff, len);

      if (rsp->str[rsp->len - 1] == '\n' &&
          rsp->str[rsp->len - 2] == '.' &&
          rsp->str[rsp->len - 3] == '\n')
        {
          g_string_truncate(rsp, rsp->len - 3);
          break;
        }

    }

  return rsp;
}

static gchar *verbose_set = NULL;

static gint
slng_verbose(int argc, char *argv[], const gchar *mode)
{
  gint ret = 0;
  GString *rsp = NULL;
  gchar buff[256], *ubuff;

  if (!verbose_set)
    snprintf(buff, 255, "LOG %s\n", mode);
  else
    snprintf(buff, 255, "LOG %s %s\n", mode,
        strncasecmp(verbose_set, "on", 2) == 0 || verbose_set[0] == '1' ? "ON" : "OFF");

  ubuff = g_ascii_strup(buff, -1);

  if (!(slng_send_cmd(ubuff) && ((rsp = slng_read_response()) != NULL)))
    {
      g_free(ubuff);
      return 1;
    }
  g_free(ubuff);

  if (!verbose_set)
    printf("%s\n", rsp->str);
  else
    ret = g_str_equal(rsp->str, "OK");

  g_string_free(rsp, TRUE);

  return ret;
}

static GOptionEntry verbose_options[] =
{
  { "set", 's', 0, G_OPTION_ARG_STRING, &verbose_set,
    "enable/disable messages", "<on|off|0|1>" },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

static gint
slng_stats(int argc, char *argv[], const gchar *mode)
{
  GString *rsp = NULL;

  if (!(slng_send_cmd("STATS\n") && ((rsp = slng_read_response()) != NULL)))
    return 1;

  printf("%s\n", rsp->str);

  g_string_free(rsp, TRUE);

  return 0;
}

static gint
slng_reload(int argc, char *argv[], const gchar *mode)
{
  GString *rsp = NULL;

  if (!(slng_send_cmd("RELOAD\n") && ((rsp = slng_read_response()) != NULL)))
    return 1;

  printf("%s\n", rsp->str);

  g_string_free(rsp, TRUE);

  return 0;
}

const gchar *
slng_mode(int *argc, char **argv[])
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
  { "stats", NULL, "Dump syslog-ng statistics", slng_stats },
  { "reload", NULL, "Reload syslog-ng", slng_reload },
  { "verbose", verbose_options, "Enable/query verbose messages", slng_verbose },
  { "debug", verbose_options, "Enable/query debug messages", slng_verbose },
  { "trace", verbose_options, "Enable/query trace messages", slng_verbose },
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

  mode_string = slng_mode(&argc, &argv);
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
          if (modes[mode].options)
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

  return modes[mode].main(argc, argv, modes[mode].mode);
}
