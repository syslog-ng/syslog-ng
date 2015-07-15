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
#include "stats/stats-json.h"
#include "stats/stats-registry.h"
#include "misc.h"

#include <json/json.h>
#include <string.h>

typedef struct json_entry
{
   gchar* name;
   json_object* value;
} json_entry;

static gboolean
has_json_special_character(const gchar *var)
{
  return FALSE;
}

static gchar *
stats_format_json_escapevar(const gchar *var)
{
  guint32 index;
  guint32 e_index;
  guint32 varlen = strlen(var);
  gchar *result, *escaped_result;

  if (varlen != 0 && has_json_special_character(var))
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

static json_object*
stats_json_object_type_with_int(void* value)
{
  const int integral_value = *((int*)(value));
  return json_object_new_int(integral_value);
}

static json_object*
stats_json_object_type_with_string(void* value)
{
  const gchar* string_value = (const gchar*) value;
  return json_object_new_string(value);
}

static json_entry*
stats_json_entry_new(const gchar* name, void* value, json_object* (*json_object_type_handler)(void*))
{
  json_entry* ret = (json_entry*) malloc(sizeof(json_entry));
  ret->name = (gchar*) malloc(strlen(name) + 1);
  strcpy(ret->name, name);
  ret->value = json_object_type_handler(value);
  return ret;
}

static void
stats_json_entry_free(json_entry* entry)
{
   free(entry->name);
}

static void
stats_json_entry_add_to_root(json_object* root, json_entry* entry)
{
  json_object_object_add(root, entry->name, entry->value);
}

static void
stats_json_entry_del_from_root(json_object* root, json_entry* entry)
{
  json_object_object_del(root, entry->name);
  stats_json_entry_free(entry);
}

static void
stats_format_json(StatsCluster *sc, gint type, StatsCounterItem *counter, gpointer user_data)
{
  GString *json = (GString *) user_data;
  gchar *s_id, *s_instance, *tag_name;
  gchar buf[32];
  gchar state[2];
  int counter_value;
  const int number_of_entries = 6;
  json_entry* entries[number_of_entries];
  json_object* root = json_object_new_object();
  int i;

  s_id = stats_format_json_escapevar(sc->id);
  s_instance = stats_format_json_escapevar(sc->instance);

  if (sc->dynamic)
    state[0] = 'd';
  else if (sc->use_count == 0)
    state[0] = 'o';
  else
    state[0] = 'a';
  state[1] = '\0';

  tag_name = stats_format_json_escapevar(stats_cluster_get_type_name(type));
  counter_value = stats_counter_get(&sc->counters[type]);

  entries[0] = stats_json_entry_new("name",
                                    stats_cluster_get_component_name(sc, buf, sizeof(buf)),
                                    stats_json_object_type_with_string);

  entries[1] =  stats_json_entry_new("id",
                                     s_id,
                                     stats_json_object_type_with_string);

  entries[2] = stats_json_entry_new("instance",
                                    s_instance,
                                    stats_json_object_type_with_string);

  entries[3] = stats_json_entry_new("state",
                                    state,
                                    stats_json_object_type_with_string);

  entries[4] = stats_json_entry_new("type",
                                    tag_name,
                                    stats_json_object_type_with_string);

  entries[5] = stats_json_entry_new("number",
                                    &counter_value,
                                    stats_json_object_type_with_int);

  for (i = 0; i < number_of_entries; ++i)
     {
       stats_json_entry_add_to_root(root, entries[i]);
     }

  g_string_append_printf(json, "%s,\n", json_object_to_json_string(root));

  for (i = 0; i < number_of_entries; ++i)
     {
        stats_json_entry_del_from_root(root, entries[i]);
     }

  g_free(tag_name);
  g_free(s_id);
  g_free(s_instance);
}

static inline
gboolean has_any_items(GString* json)
{
   return json->len > 1;
}

gchar *
stats_generate_json(void)
{
  GString *json= g_string_sized_new(1024);

  g_string_append_printf(json, "[");
  stats_lock();
  stats_foreach_counter(stats_format_json, json);
  stats_unlock();
  if (has_any_items(json))
    {
      /* removing the ',' and '\n' after the end of the last json structure */
      g_string_truncate(json, json->len-2);
    }
  g_string_append_printf(json, "]");

  return g_string_free(json, FALSE);
}
