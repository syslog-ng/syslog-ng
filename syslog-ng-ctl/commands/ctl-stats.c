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
static gboolean stats_options_without_header = FALSE;
static gchar *stats_options_out_format = NULL;
static gchar **stats_legacy_commands = NULL;

static const gchar *
_stats_command_builder(GOptionContext *ctx)
{
  if (stats_options_reset_is_set)
    return "RESET_STATS";

  if (stats_options_remove_orphans)
    return "REMOVE_ORPHANED_STATS";

  /* NOTE: Yes, this is a mess, but it's a legacy mess, so we have to keep it currently to be backward compatible.
   *       Unfortunately, there's no optional command construct in the GLib Commandline Option Parser
   *       so, we cannot use the automatic way to parse the sub-command arguments with options.
   *       See slng_attach() for a better example, how easy it could be on the proper, automatic way.
   */
  const gchar *stats_command = stats_legacy_commands ? stats_legacy_commands[0] : NULL;

  if (stats_command && stats_legacy_commands[1]) // more than one sub-command
    {
      g_option_context_set_description(ctx, "stats");
      fprintf(stderr, "Error parsing command line arguments: Invalid stats command\n");
      return NULL;
    }

  gboolean command_is_csv = stats_command && g_str_equal(stats_command, "csv");
  gboolean command_is_kv = stats_command && g_str_equal(stats_command, "kv");
  gboolean command_is_prometheus = stats_command && g_str_equal(stats_command, "prometheus");
  gboolean option_is_csv = stats_options_out_format && g_str_equal(stats_options_out_format, "csv");
  gboolean option_is_kv = stats_options_out_format && g_str_equal(stats_options_out_format, "kv");
  gboolean option_is_prometheus = stats_options_out_format && g_str_equal(stats_options_out_format, "prometheus");

  if (stats_command && FALSE == command_is_csv && FALSE == command_is_kv && FALSE == command_is_prometheus)
    {
      g_option_context_set_description(ctx, "stats");
      fprintf(stderr, "Error parsing command line arguments: Unknown stats format, %s\n", stats_command);
      return NULL;
    }

  if (stats_options_out_format && FALSE == option_is_csv && FALSE == option_is_kv && FALSE == option_is_prometheus)
    {
      g_option_context_set_description(ctx, "stats");
      fprintf(stderr, "Error parsing command line arguments: Unknown stats format, %s\n", stats_options_out_format);
      return NULL;
    }

  if ((stats_command == NULL && stats_options_out_format == NULL) ||
      command_is_csv || option_is_csv)
    {
      if (option_is_prometheus || command_is_prometheus || option_is_kv || command_is_kv)
        goto on_collision;
      if (stats_options_without_header)
        return "STATS CSV WITHOUT_HEADER";
      else
        return "STATS CSV";
    }
  else if (command_is_kv || option_is_kv)
    {
      if (option_is_csv || command_is_csv || option_is_prometheus || command_is_prometheus)
        goto on_collision;
      return "STATS KV";
    }
  else if (command_is_prometheus || option_is_prometheus)
    {
      if (option_is_csv || command_is_csv || option_is_kv || command_is_kv)
        goto on_collision;
      if (stats_options_legacy_metrics)
        return "STATS PROMETHEUS WITH_LEGACY";
      else
        return "STATS PROMETHEUS";
    }

on_collision:
  g_option_context_set_description(ctx, "stats");
  fprintf(stderr,
          "Error parsing command line arguments: Colliding stats format recieved: %s <-> %s\nUsing the `stats csv|kv|stats prometheus` format is deprecated, please use the new `--%s csv|--%s kv|--%s prometheus` options instead\n\n",
          stats_options_out_format, stats_command,
          stats_options[3].long_name, stats_options[3].long_name, stats_options[3].long_name);
  return NULL;
}

gint
slng_stats(int argc, char *argv[], const gchar *mode, GOptionContext *ctx)
{
  const gchar *command = _stats_command_builder(ctx);
  if (command == NULL)
    return EINVAL;

  return dispatch_command(command);
}

GOptionEntry stats_options[] =
{
  { "reset", 'r', 0, G_OPTION_ARG_NONE, &stats_options_reset_is_set, "reset counters", NULL },
  { "remove-orphans", 'o', 0, G_OPTION_ARG_NONE, &stats_options_remove_orphans, "remove orphaned statistics", NULL},
  { "with-legacy-metrics", 'l', 0, G_OPTION_ARG_NONE, &stats_options_legacy_metrics, "show legacy metrics (only for prometheus format)", NULL},
  { "no-header", 'n', 0, G_OPTION_ARG_NONE, &stats_options_without_header, "do not print stats header", NULL},
  { "format", 'm', 0, G_OPTION_ARG_STRING, &stats_options_out_format, "output format, default is <csv>", "<csv|kv|prometheus>" },
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &stats_legacy_commands, NULL, NULL },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
};
