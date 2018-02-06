/*
 * Copyright (c) 2002-2017 Balabit
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
#include <locale.h>
#include <sgtty.h>
#include <termios.h>
#include <unistd.h>

#if SYSLOG_NG_HAVE_GETOPT_H
#include <getopt.h>
#endif

typedef struct _CommandDescriptor
{
  const gchar *mode;
  const GOptionEntry *options;
  const gchar *description;
  gint (*main)(gint argc, gchar *argv[], const gchar *mode, GOptionContext *ctx);
  struct _CommandDescriptor *subcommands;
} CommandDescriptor;

static const gchar *control_name;
static ControlClient *control_client;
static void print_usage(const gchar *bin_name, CommandDescriptor *descriptors);

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

static gboolean
_is_response_empty(GString *response)
{
  return (response == NULL || g_str_equal(response->str, ""));
}

static gint
_dispatch_command(const gchar *cmd)
{
  if (!cmd)
    return 1;

  gchar *dispatchable_command = g_strdup_printf("%s\n", cmd);
  GString *rsp = slng_run_command(dispatchable_command);
  gint result;

  if (!rsp || _is_response_empty(rsp))
    {
      result = 1;
    }
  else
    {
      printf("%s\n", rsp->str);
      result = 0;
    }

  //Fill buffers with '?' to avoid security sensitive data remanins on heap
  if (rsp)
    {
      g_string_overwrite(rsp,rsp->len,"?");
      g_string_free(rsp, TRUE);
    }

  memset(dispatchable_command,'?',strlen(dispatchable_command));
  g_free(dispatchable_command);

  return result;
}

static gchar *verbose_set = NULL;

static GOptionEntry verbose_options[] =
{
  {
    "set", 's', 0, G_OPTION_ARG_STRING, &verbose_set,
    "enable/disable messages", "<on|off|0|1>"
  },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

static gint
slng_verbose(int argc, char *argv[], const gchar *mode, GOptionContext *ctx)
{
  gint ret = 0;
  GString *rsp = NULL;
  gchar buff[256];

  if (!verbose_set)
    g_snprintf(buff, 255, "LOG %s\n", mode);
  else
    g_snprintf(buff, 255, "LOG %s %s\n", mode,
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

static const gchar *
_stats_command_builder(void)
{
  return stats_options_reset_is_set ? "RESET_STATS" : "STATS";
}

static gint
slng_stats(int argc, char *argv[], const gchar *mode, GOptionContext *ctx)
{
  return _dispatch_command(_stats_command_builder());
}

static gint
slng_stop(int argc, char *argv[], const gchar *mode, GOptionContext *ctx)
{
  return _dispatch_command("STOP");
}

static gint
slng_reload(int argc, char *argv[], const gchar *mode, GOptionContext *ctx)
{
  return _dispatch_command("RELOAD");
}

static gint
slng_reopen(int argc, char *argv[], const gchar *mode, GOptionContext *ctx)
{
  return _dispatch_command("REOPEN");
}

const static gint QUERY_COMMAND = 0;
static gboolean query_is_get_sum = FALSE;
static gboolean query_reset = FALSE;
static gchar **raw_query_params = NULL;

static GOptionEntry query_options[] =
{
  { "sum", 0, 0, G_OPTION_ARG_NONE, &query_is_get_sum, "aggregate sum", NULL },
  { "reset", 0, 0, G_OPTION_ARG_NONE, &query_reset, "reset counters after query", NULL },
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &raw_query_params, NULL, NULL },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
};

enum
{
  QUERY_CMD_LIST,
  QUERY_CMD_LIST_RESET,
  QUERY_CMD_GET,
  QUERY_CMD_GET_RESET,
  QUERY_CMD_GET_SUM,
  QUERY_CMD_GET_SUM_RESET
};

const static gchar *QUERY_COMMANDS[] = {"LIST", "LIST_RESET", "GET", "GET_RESET", "GET_SUM", "GET_SUM_RESET"};


static gboolean license_json = FALSE;

static GOptionEntry license_options[] =
{
  { "json", 'J', 0, G_OPTION_ARG_NONE, &license_json, "enable json output", NULL },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

static gint
slng_license(int argc, char *argv[], const gchar *mode, GOptionContext *ctx)
{
  GString *rsp = NULL;
  gchar buff[256];

  if (license_json)
    g_snprintf(buff, 255, "LICENSE JSON\n");
  else
    g_snprintf(buff, 255, "LICENSE\n");

  if (!(slng_send_cmd(buff) && ((rsp = control_client_read_reply(control_client)) != NULL)))
    return 1;

  printf("%s\n", rsp->str);

  g_string_free(rsp, TRUE);

  return 0;
}

static gint
_get_query_list_cmd(void)
{
  if (query_is_get_sum)
    return -1;

  if (query_reset)
    return QUERY_CMD_LIST_RESET;

  return QUERY_CMD_LIST;
}

static gint
_get_query_get_cmd(void)
{
  if (query_is_get_sum)
    {
      if (query_reset)
        return QUERY_CMD_GET_SUM_RESET;

      return QUERY_CMD_GET_SUM;
    }

  if (query_reset)
    return QUERY_CMD_GET_RESET;

  return QUERY_CMD_GET;

}

static gint
_get_query_cmd(gchar *cmd)
{
  if (g_str_equal(cmd, "list"))
    return _get_query_list_cmd();

  if (g_str_equal(cmd, "get"))
    return _get_query_get_cmd();

  return -1;
}

static gboolean
_is_query_params_empty(void)
{
  return raw_query_params == NULL;
}

static gchar *
_shift_query_command_out_of_params(void)
{
  if (raw_query_params[QUERY_COMMAND] != NULL)
    return *(raw_query_params++);
  return *raw_query_params;
}

static gboolean
_validate_get_params(gint query_cmd)
{
  if(query_cmd == QUERY_CMD_GET || query_cmd == QUERY_CMD_GET_SUM)
    if (*raw_query_params == NULL)
      {
        fprintf(stderr, "error: need a path argument\n");
        return TRUE;
      }
  return FALSE;
}

static gchar *
_get_query_command_string(gint query_cmd)
{
  gchar *query_params_to_pass, *command_to_dispatch;
  query_params_to_pass = g_strjoinv(" ", raw_query_params);
  if (query_params_to_pass)
    {
      command_to_dispatch = g_strdup_printf("QUERY %s %s", QUERY_COMMANDS[query_cmd], query_params_to_pass);
    }
  else
    {
      command_to_dispatch = g_strdup_printf("QUERY %s", QUERY_COMMANDS[query_cmd]);
    }
  g_free(query_params_to_pass);

  return command_to_dispatch;
}

static gchar *
_get_dispatchable_query_command(void)
{
  gint query_cmd;

  if (_is_query_params_empty())
    return NULL;

  query_cmd = _get_query_cmd(raw_query_params[QUERY_COMMAND]);
  if (query_cmd < 0)
    return NULL;

  *raw_query_params = _shift_query_command_out_of_params();
  if(_validate_get_params(query_cmd))
    return NULL;

  return _get_query_command_string(query_cmd);
}

static gint
slng_query(int argc, char *argv[], const gchar *mode, GOptionContext *ctx)
{
  gint result;

  gchar *cmd = _get_dispatchable_query_command();
  if (cmd == NULL)
    return 1;

  result = _dispatch_command(cmd);

  g_free(cmd);

  return result;
}

static GOptionEntry no_options[] =
{
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

static void
console_echo_on(gboolean turn_on)
{
  struct termios t;

  if (tcgetattr(STDIN_FILENO, &t))
    return;

  if (turn_on)
    t.c_lflag |= ECHO;
  else
    t.c_lflag &= ~((tcflag_t) ECHO);

  tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

static void
get_password_from_stdin(gchar *buffer, gulong *length)
{
  printf("enter password:");
  console_echo_on(FALSE);
  getline(&buffer,length,stdin);
  console_echo_on(TRUE);
}

static gint
slng_passwd(int argc, char *argv[], const gchar *mode, GOptionContext *ctx)
{
  gulong buff_size = 255;
  gchar buff[buff_size+1];

  if (g_strcmp0(mode,"password-add")==0)
    {
      if (argc<2 || argv[1]==NULL || strlen(argv[1])==0)
        {
          gchar *usage = g_option_context_get_help(ctx, TRUE, NULL);
          fprintf(stderr, "Error: missing arguments!\n%s\n", usage);
          g_free(usage);
          return 1;
        }

      if (argc<3 || argv[2]==NULL || strlen(argv[2])==0)
        {
          gchar *password_buffer = g_malloc0((gsize)buff_size+1);
          if (!password_buffer)
            return 1;

          get_password_from_stdin(password_buffer,&buff_size);
          g_snprintf(buff, buff_size, "PWD %s %s %s", "add", argv[1], password_buffer);
          memset(password_buffer,0,buff_size);
          g_free(password_buffer);
        }
      else
        {
          g_snprintf(buff, buff_size, "PWD %s %s %s", "add", argv[1], argv[2]);
        }
    }
  else if (g_strcmp0(mode,"password-status")==0)
    {
      g_snprintf(buff, buff_size, "PWD %s", "status");
    }
  else
    {
      gchar *usage = g_option_context_get_help(ctx, TRUE, NULL);
      fprintf(stderr, "Error: missing arguments!\n%s\n", usage);
      g_free(usage);
      return 1;
    }

  gint result = _dispatch_command(buff);

  //Fill buffers with 0x00 to avoid security sensitive data remanins on stack/heap
  memset(buff,0,buff_size);
  if (argc > 2 && argv[2]) memset(argv[2],0,strlen(argv[2]));
  return result;
}

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
  {
    "control", 'c', 0, G_OPTION_ARG_STRING, &control_name,
    "syslog-ng control socket", "<socket>"
  },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

static CommandDescriptor modes[] =
{
  { "stats", stats_options, "Get syslog-ng statistics in CSV format", slng_stats, NULL },
  { "verbose", verbose_options, "Enable/query verbose messages", slng_verbose, NULL },
  { "debug", verbose_options, "Enable/query debug messages", slng_verbose, NULL },
  { "trace", verbose_options, "Enable/query trace messages", slng_verbose, NULL },
  { "stop", no_options, "Stop syslog-ng process", slng_stop, NULL },
  { "reload", no_options, "Reload syslog-ng", slng_reload, NULL },
  { "reopen", no_options, "Re-open of log destination files", slng_reopen, NULL },
  { "query", query_options, "Query syslog-ng statistics. Possible commands: list, get, get --sum", slng_query, NULL },
  { "show-license-info", license_options, "Show information about the license", slng_license, NULL },
  { "password-add", no_options, "Add key-password pairs. syslog-ng-ctl password-add key [password]", slng_passwd, NULL },
  { "password-status", no_options, "Query stored key/password status)", slng_passwd, NULL },
  { NULL, NULL },
};

static void
print_usage(const gchar *bin_name, CommandDescriptor *descriptors)
{
  gint mode;

  fprintf(stderr, "Syntax: %s <command> [options]\nPossible commands are:\n", bin_name);
  for (mode = 0; descriptors[mode].mode; mode++)
    {
      fprintf(stderr, "    %-20s %s\n", descriptors[mode].mode, descriptors[mode].description);
    }
}

gboolean
_is_help(gchar *cmd)
{
  return g_str_equal(cmd, "--help");
}


static CommandDescriptor *
find_active_mode(const gchar *mode_string, CommandDescriptor descriptors[])
{
  for (gint mode = 0; modes[mode].mode; mode++)
    if (strcmp(modes[mode].mode, mode_string) == 0)
      return &descriptors[mode];
  return NULL;
}

static GOptionContext *
setup_help_context(const gchar *mode_string, CommandDescriptor *active_mode)
{
  if (!active_mode)
    return NULL;

  GOptionContext *ctx = g_option_context_new(mode_string);
#if GLIB_CHECK_VERSION (2, 12, 0)
  g_option_context_set_summary(ctx, active_mode->description);
#endif
  g_option_context_add_main_entries(ctx, active_mode->options, NULL);
  g_option_context_add_main_entries(ctx, slng_options, NULL);

  return ctx;
}

int
main(int argc, char *argv[])
{
  const gchar *mode_string;
  GError *error = NULL;
  int result;

  setlocale(LC_ALL, "");

  control_name = get_installation_path_for(PATH_CONTROL_SOCKET);

  if (argc > 1 && _is_help(argv[1]))
    {
      print_usage(argv[0], modes);
      exit(0);
    }

  mode_string = get_mode(&argc, &argv);
  if (!mode_string)
    {
      print_usage(argv[0], modes);
      exit(1);
    }

  CommandDescriptor *active_mode = find_active_mode(mode_string, modes);
  GOptionContext *ctx = setup_help_context(mode_string, active_mode);

  if (!ctx)
    {
      fprintf(stderr, "Unknown command\n");
      print_usage(argv[0], modes);
      exit(1);
    }

  if (!g_option_context_parse(ctx, &argc, &argv, &error))
    {
      fprintf(stderr, "Error parsing command line arguments: %s\n", error ? error->message : "Invalid arguments");
      g_clear_error(&error);
      g_option_context_free(ctx);
      return 1;
    }

  control_client = control_client_new(control_name);
  if (control_client_connect(control_client))
    result = active_mode->main(argc, argv, active_mode->mode, ctx);
  else
    result = FALSE;

  g_option_context_free(ctx);

  control_client_free(control_client);
  return result;
}
