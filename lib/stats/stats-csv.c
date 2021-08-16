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
#include "stats/stats-csv.h"
#include "stats/stats-registry.h"
#include "stats/stats-cluster.h"
#include "utf8utils.h"

#include <string.h>

static gboolean
has_csv_special_character(const gchar *var)
{
  if (strchr(var, ';'))
    return TRUE;
  if (strchr(var, '\"'))
    return TRUE;
  return FALSE;
}

static gchar *
stats_format_csv_escapevar(const gchar *var)
{
  gchar *escaped_result;

  if (var[0] && has_csv_special_character(var))
    {
      gchar *result;
      /* quote */
      result = convert_unsafe_utf8_to_escaped_binary(var, -1, "\"");
      escaped_result = g_strdup_printf("\"%s\"", result);
      g_free(result);
    }
  else
    {
      escaped_result = convert_unsafe_utf8_to_escaped_binary(var, -1, NULL);
    }
  return escaped_result;
}

static void
stats_format_csv(StatsCluster *sc, gint type, StatsCounterItem *counter, gpointer user_data)
{
  GString *csv = (GString *) user_data;
  gchar *s_id, *s_instance, *tag_name;
  gchar buf[32];
  gchar state;

  s_id = stats_format_csv_escapevar(sc->key.id);
  s_instance = stats_format_csv_escapevar(sc->key.instance);

  if (sc->dynamic)
    state = 'd';
  else if (stats_cluster_is_orphaned(sc))
    state = 'o';
  else
    state = 'a';

  tag_name = stats_format_csv_escapevar(stats_cluster_get_type_name(sc, type));
  g_string_append_printf(csv, "%s;%s;%s;%c;%s;%"G_GSIZE_FORMAT"\n",
                         stats_cluster_get_component_name(sc, buf, sizeof(buf)),
                         s_id, s_instance, state, tag_name, stats_counter_get(&sc->counter_group.counters[type]));
  g_free(tag_name);
  g_free(s_id);
  g_free(s_instance);
}


gchar *
stats_generate_csv(void)
{
  GString *csv = g_string_sized_new(1024);

  g_string_append_printf(csv, "%s;%s;%s;%s;%s;%s\n", "SourceName", "SourceId", "SourceInstance", "State", "Type",
                         "Number");
  stats_lock();
  stats_foreach_counter(stats_format_csv, csv);
  stats_unlock();
  return g_string_free(csv, FALSE);
}
