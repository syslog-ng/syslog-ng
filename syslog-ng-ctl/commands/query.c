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

#include "query.h"

static const gint QUERY_COMMAND = 0;
static gboolean query_is_get_sum = FALSE;
static gboolean query_reset = FALSE;
static gchar **raw_query_params = NULL;

GOptionEntry query_options[] =
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

static const gchar *QUERY_COMMANDS[] = {"LIST", "LIST_RESET", "GET", "GET_RESET", "GET_SUM", "GET_SUM_RESET"};

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

static void
_shift_query_command_out_of_params(void)
{
  if (raw_query_params[QUERY_COMMAND] != NULL)
    ++raw_query_params;
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

  _shift_query_command_out_of_params();
  if(_validate_get_params(query_cmd))
    return NULL;

  return _get_query_command_string(query_cmd);
}

gint
slng_query(int argc, char *argv[], const gchar *mode, GOptionContext *ctx)
{
  gint result;

  gchar *cmd = _get_dispatchable_query_command();
  if (cmd == NULL)
    return 1;

  result = dispatch_command(cmd);

  g_free(cmd);

  return result;
}
