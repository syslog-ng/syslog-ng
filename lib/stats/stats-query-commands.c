/*
 * Copyright (c) 2017 Balabit
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

#include "stats-query-commands.h"
#include "stats/stats-query.h"
#include "messages.h"

typedef enum _QueryCommand
{
  QUERY_GET = 0,
  QUERY_GET_SUM,
  QUERY_LIST,
  QUERY_CMD_MAX
} QueryCommand;

typedef GString *(*query_cmd)(const gchar *filter_expr);

static GString *
_query_get(const gchar *filter_expr)
{
  return stats_query_get(filter_expr);
}

static GString *
_query_list(const gchar *filter_expr)
{
  return stats_query_list(filter_expr);
}

static GString *
_query_get_sum(const gchar *filter_expr)
{
  return stats_query_get_sum(filter_expr);
}

static QueryCommand
_command_str_to_id(const gchar *cmd)
{
  if (g_str_equal(cmd, "GET_SUM"))
    return QUERY_GET_SUM;

  if (g_str_equal(cmd, "GET"))
    return QUERY_GET;

  if (g_str_equal(cmd, "LIST"))
    return QUERY_LIST;

  msg_error("Unknown query command", evt_tag_str("command", cmd));

  return QUERY_CMD_MAX;
}

static query_cmd QUERY_CMDS[] =
{
  _query_get,
  _query_get_sum,
  _query_list
};

static GString *
_dispatch_query(gint cmd_id, const gchar *filter_expr)
{
  if (cmd_id < QUERY_GET || cmd_id >= QUERY_CMD_MAX)
    {
      msg_error("Invalid query command",
                evt_tag_int("cmd_id", cmd_id),
                evt_tag_str("query", filter_expr));
      return g_string_new("");
    }

  return QUERY_CMDS[cmd_id](filter_expr);
}

GString *
process_query_command(GString *command, gpointer user_data)
{
  GString *result;
  gchar **cmds = g_strsplit(command->str, " ", 3);

  g_assert(g_str_equal(cmds[0], "QUERY"));

  result = _dispatch_query(_command_str_to_id(cmds[1]), cmds[2]);

  g_strfreev(cmds);
  return result;
}

