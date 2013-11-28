/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
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
#include "misc.h"

#include <string.h>

static gboolean
has_csv_special_character(const gchar *var)
{
  gchar *p1 = strchr(var, ';');
  if (p1)
    return TRUE;
  p1 = strchr(var, '\n');
  if (p1)
    return TRUE;
  if (var[0] == '"')
    return TRUE;
  return FALSE;
}

static gchar *
stats_format_csv_escapevar(const gchar *var)
{
  guint32 index;
  guint32 e_index;
  guint32 varlen = strlen(var);
  gchar *result, *escaped_result;

  if (varlen != 0 && has_csv_special_character(var))
    {
      result = g_malloc(varlen*2);

      result[0] = '"';
      e_index = 1;
      for (index = 0; index < varlen; index++)
        {
          if (var[index] == '"')
            {
              result[e_index] = '\\';
              e_index++;
            }
          result[e_index] = var[index];
          e_index++;
        }
      result[e_index] = '"';
      result[e_index + 1] = 0;

      escaped_result = utf8_escape_string(result, e_index + 2);
      g_free(result);
    }
  else
    {
      escaped_result = utf8_escape_string(var, strlen(var));
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

  s_id = stats_format_csv_escapevar(sc->id);
  s_instance = stats_format_csv_escapevar(sc->instance);
  
  if (sc->dynamic)
    state = 'd';
  else if (sc->ref_cnt == 0)
    state = 'o';
  else
    state = 'a';

  tag_name = stats_format_csv_escapevar(stats_cluster_get_type_name(type));
  g_string_append_printf(csv, "%s;%s;%s;%c;%s;%u\n",
                         stats_cluster_get_component_name(sc, buf, sizeof(buf)),
                         s_id, s_instance, state, tag_name, stats_counter_get(&sc->counters[type]));
  g_free(tag_name);
  g_free(s_id);
  g_free(s_instance);
}


gchar *
stats_generate_csv(void)
{
  GString *csv = g_string_sized_new(1024);

  g_string_append_printf(csv, "%s;%s;%s;%s;%s;%s\n", "SourceName", "SourceId", "SourceInstance", "State", "Type", "Number");
  stats_lock();
  stats_foreach_counter(stats_format_csv, csv);
  stats_unlock();
  return g_string_free(csv, FALSE);
}
