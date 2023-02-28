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

#include "ctl-stats.h"
#include "syslog-ng.h"

static gboolean stats_options_reset_is_set = FALSE;
static gboolean stats_options_remove_orphans = FALSE;
static gboolean stats_options_legacy_metrics = FALSE;
static gchar **stats_commands = NULL;

GOptionEntry stats_options[] =
{
  { "reset", 'r', 0, G_OPTION_ARG_NONE, &stats_options_reset_is_set, "reset counters", NULL },
  { "remove-orphans", 'o', 0, G_OPTION_ARG_NONE, &stats_options_remove_orphans, "remove orphaned statistics", NULL},
  { "with-legacy-metrics", 'l', 0, G_OPTION_ARG_NONE, &stats_options_legacy_metrics, "show legacy metrics", NULL},
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &stats_commands, NULL, NULL },
  { NULL,    0,   0, G_OPTION_ARG_NONE, NULL,                        NULL,             NULL }
};

static const gchar *
_stats_command_builder(void)
{
  if (stats_options_reset_is_set)
    return "RESET_STATS";

  if (stats_options_remove_orphans)
    return "REMOVE_ORPHANED_STATS";

  const gchar *stats_command = stats_commands ? stats_commands[0] : NULL;
  if (!stats_command || g_str_equal(stats_command, "csv"))
    return "STATS CSV";

  if (g_str_equal(stats_command, "prometheus"))
    {
      if (stats_options_legacy_metrics)
        return "STATS PROMETHEUS WITH_LEGACY";
      else
        return "STATS PROMETHEUS";
    }

  return NULL;
}

gint
slng_stats(int argc, char *argv[], const gchar *mode, GOptionContext *ctx)
{
  const gchar *command = _stats_command_builder();
  if (!command)
    return 1;

  return dispatch_command(command);
}
