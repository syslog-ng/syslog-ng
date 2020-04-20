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

#include "messages.h"
#include "stats-query-commands.h"
#include "stats/stats-query.h"
#include "control/control-server.h"

typedef enum _QueryCommand
{
  QUERY_GET = 0,
  QUERY_GET_RESET,
  QUERY_GET_SUM,
  QUERY_GET_SUM_RESET,
  QUERY_LIST,
  QUERY_LIST_RESET,
  QUERY_CMD_MAX
} QueryCommand;

typedef gboolean (*query_cmd)(const gchar *filter_expr, GString *result);

const gint CMD_STR = 0;
const gint QUERY_CMD_STR = 1;
const gint QUERY_FILTER_STR = 2;


static void
_append_reset_msg_if_found_matching_counters(gboolean found_match, GString *result)
{
  if (found_match)
    {
      g_string_append_printf(result, "\n%s", "The selected counters of syslog-ng have been reset to 0.");
    }
}

static gboolean
_ctl_format_get(StatsCounterItem *ctr, gpointer user_data)
{
  GString *str = (GString *)user_data;
  g_string_append_printf(str, "%s=%"G_GSIZE_FORMAT"\n", stats_counter_get_name(ctr), stats_counter_get(ctr));
  return TRUE;
}

static gboolean
_ctl_format_name_without_value(StatsCounterItem *ctr, gpointer user_data)
{
  GString *str = (GString *)user_data;
  g_string_append_printf(str, "%s\n", stats_counter_get_name(ctr));
  return TRUE;
}

static gboolean
_ctl_format_get_sum(gpointer user_data)
{
  gpointer *args = (gpointer *) user_data;
  GString *result = (GString *) args[0];
  gint64 *sum = (gint64 *) args[1];

  g_string_printf(result, "%" G_GINT64_FORMAT "\n", *sum);

  return TRUE;
}

static gboolean
_query_get(const gchar *filter_expr, GString *result)
{
  return stats_query_get(filter_expr, _ctl_format_get, (gpointer)result);
}

static gboolean
_query_get_and_reset(const gchar *filter_expr, GString *result)
{
  gboolean found_match;

  found_match = stats_query_get_and_reset_counters(filter_expr, _ctl_format_get, (gpointer)result);
  _append_reset_msg_if_found_matching_counters(found_match, result);

  return found_match;
}

static gboolean
_query_list(const gchar *filter_expr, GString *result)
{
  return stats_query_list(filter_expr, _ctl_format_name_without_value, (gpointer)result);
}

static gboolean
_query_list_and_reset(const gchar *filter_expr, GString *result)
{
  gboolean found_match;

  found_match = stats_query_list_and_reset_counters(filter_expr, _ctl_format_name_without_value, (gpointer)result);
  _append_reset_msg_if_found_matching_counters(found_match, result);

  return found_match;
}

static gboolean
_query_get_sum(const gchar *filter_expr, GString *result)
{
  return stats_query_get_sum(filter_expr, _ctl_format_get_sum, (gpointer)result);
}

static gboolean
_query_get_sum_and_reset(const gchar *filter_expr, GString *result)
{
  gboolean found_match;

  found_match = stats_query_get_sum_and_reset_counters(filter_expr, _ctl_format_get_sum, (gpointer)result);
  _append_reset_msg_if_found_matching_counters(found_match, result);

  return found_match;
}

static QueryCommand
_command_str_to_id(const gchar *cmd)
{
  if (g_str_equal(cmd, "GET_SUM"))
    return QUERY_GET_SUM;

  if (g_str_equal(cmd, "GET_SUM_RESET"))
    return QUERY_GET_SUM_RESET;

  if (g_str_equal(cmd, "GET"))
    return QUERY_GET;

  if (g_str_equal(cmd, "GET_RESET"))
    return QUERY_GET_RESET;

  if (g_str_equal(cmd, "LIST"))
    return QUERY_LIST;

  if (g_str_equal(cmd, "LIST_RESET"))
    return QUERY_LIST_RESET;

  msg_error("Unknown query command", evt_tag_str("command", cmd));

  return QUERY_CMD_MAX;
}

static query_cmd QUERY_CMDS[] =
{
  _query_get,
  _query_get_and_reset,
  _query_get_sum,
  _query_get_sum_and_reset,
  _query_list,
  _query_list_and_reset
};

static gboolean
_dispatch_query(gint cmd_id, const gchar *filter_expr, GString *result)
{
  if (cmd_id < QUERY_GET || cmd_id >= QUERY_CMD_MAX)
    {
      msg_error("Invalid query command",
                evt_tag_int("cmd_id", cmd_id),
                evt_tag_str("query", filter_expr));
      return FALSE;
    }

  return QUERY_CMDS[cmd_id](filter_expr, result);
}

void
process_query_command(ControlConnection *cc, GString *command, gpointer user_data)
{
  GString *result = g_string_new("");
  gchar **cmds = g_strsplit(command->str, " ", 3);

  g_assert(g_str_equal(cmds[CMD_STR], "QUERY"));

  _dispatch_query(_command_str_to_id(cmds[QUERY_CMD_STR]), cmds[QUERY_FILTER_STR], result);

  g_strfreev(cmds);

  if (result->len == 0)
    g_string_assign(result, "\n");

  control_connection_send_reply(cc, result);
}
