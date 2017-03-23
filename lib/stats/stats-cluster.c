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
#include "stats/stats-cluster.h"

#include <string.h>

typedef struct _StatsComponentsElement
{
  GString *name;
  gint index;
} StatsComponentsElement;

static GHashTable *stats_components = NULL;
static StatsComponentsElement *mapping_from_component_number_to_element[SCS_SOURCE_MASK];

static guint
_stats_components_element_hash_cb(gconstpointer element)
{
  if (element)
    return g_str_hash(((StatsComponentsElement *)element)->name->str);

  return 0;
}

static gboolean
_stats_components_names_are_equal_cb(gconstpointer a, gconstpointer b)
{
  return g_strcmp0(((StatsComponentsElement *)a)->name->str, ((StatsComponentsElement *)b)->name->str) == 0;
}

static void
_stats_components_destroy_element_cb(gpointer e)
{
  StatsComponentsElement *element = (StatsComponentsElement *)e;
  if (element)
    {
      g_string_free(element->name, TRUE);
      g_free(element);
    }
}
void
stats_components_init()
{
  gint i;

  if (!stats_components)
    stats_components = g_hash_table_new_full(_stats_components_element_hash_cb,
                                             _stats_components_names_are_equal_cb,
                                             NULL,
                                             _stats_components_destroy_element_cb);

  for (i = 0; i < SCS_SOURCE_MASK; i++)
    {
      mapping_from_component_number_to_element[i] = NULL;
    }
}

void
stats_components_deinit()
{
  gint i;
  g_hash_table_destroy(stats_components);
  stats_components = NULL;

  for (i = 0; i < SCS_SOURCE_MASK; i++)
    mapping_from_component_number_to_element[i] = NULL;
}

int
stats_components_get_component_index(const gchar *name)
{
  GString *name_str;
  StatsComponentsElement *new_element;
  StatsComponentsElement *searched_element;
  int comp_index;


  name_str = g_string_new(name);
  new_element = g_new0(StatsComponentsElement, 1);
  new_element->name = name_str;
  searched_element = g_hash_table_lookup(stats_components, new_element);

  if (searched_element)
    {
      comp_index = ((StatsComponentsElement *)searched_element)->index;
      g_string_free(name_str, TRUE);
      g_free(new_element);
    }
  else
    {
      comp_index = g_hash_table_size(stats_components);
      g_hash_table_add(stats_components, new_element);
      new_element->index = comp_index;

      if (mapping_from_component_number_to_element[comp_index])
        {
          g_assert_not_reached();
        }
      mapping_from_component_number_to_element[comp_index] = new_element;
    }

  return comp_index;
}

void
stats_cluster_foreach_counter(StatsCluster *self, StatsForeachCounterFunc func, gpointer user_data)
{
  gint type;

  for (type = 0; type < SC_TYPE_MAX; type++)
    {
      if (self->live_mask & (1 << type))
        {
          func(self, type, &self->counters[type], user_data);
        }
    }
}

static const gchar *tag_names[SC_TYPE_MAX] =
{
  /* [SC_TYPE_DROPPED]   = */ "dropped",
  /* [SC_TYPE_PROCESSED] = */ "processed",
  /* [SC_TYPE_STORED]   = */  "stored",
  /* [SC_TYPE_SUPPRESSED] = */ "suppressed",
  /* [SC_TYPE_STAMP] = */ "stamp",
};

const gchar *
stats_cluster_get_type_name(gint type)
{
  return tag_names[type];
}

static const gchar *
_get_module_name(gint source)
{
  static const gchar *module_names[SCS_MAX] =
  {
    "none",
    "file",
    "pipe",
    "tcp",
    "udp",
    "tcp6",
    "udp6",
    "unix-stream",
    "unix-dgram",
    "syslog",
    "network",
    "internal",
    "logstore",
    "program",
    "sql",
    "sun-streams",
    "usertty",
    "group",
    "center",
    "host",
    "global",
    "mongodb",
    "class",
    "rule_id",
    "tag",
    "severity",
    "facility",
    "sender",
    "smtp",
    "amqp",
    "stomp",
    "redis",
    "snmp",
    "riemann",
    "journald",
    "java",
    "http"
  };
  return module_names[source & SCS_SOURCE_MASK];
}


static const gchar *
_get_component_prefix(gint source)
{
  return (source & SCS_SOURCE ? "src." : (source & SCS_DESTINATION ? "dst." : ""));
}

/* buf is a scratch area which is not always used, the return value is
 * either a locally managed string or points to @buf.  */
const gchar *
stats_cluster_get_component_name(StatsCluster *self, gchar *buf, gsize buf_len)
{
  StatsComponentsElement *element;

  if ((self->component & SCS_SOURCE_MASK) == stats_components_get_component_index("group"))
    {
      if (self->component & SCS_SOURCE)
        return "source";
      else if (self->component & SCS_DESTINATION)
        return "destination";
      else
        g_assert_not_reached();
    }
  else
    {
      element = mapping_from_component_number_to_element[self->component & SCS_SOURCE_MASK];
      if (!element)
        g_assert_not_reached();

      g_snprintf(buf, buf_len, "%s%s",
                 _get_component_prefix(self->component),
                 ((StatsComponentsElement *)element)->name->str);
      return buf;
    }
}

gboolean
stats_cluster_equal(const StatsCluster *sc1, const StatsCluster *sc2)
{
  return sc1->component == sc2->component && strcmp(sc1->id, sc2->id) == 0 && strcmp(sc1->instance, sc2->instance) == 0;
}

guint
stats_cluster_hash(const StatsCluster *self)
{
  return g_str_hash(self->id) + g_str_hash(self->instance) + self->component;
}

StatsCounterItem *
stats_cluster_track_counter(StatsCluster *self, gint type)
{
  gint type_mask = 1 << type;

  g_assert(type < SC_TYPE_MAX);

  self->live_mask |= type_mask;
  self->use_count++;
  return &self->counters[type];
}

void
stats_cluster_untrack_counter(StatsCluster *self, gint type, StatsCounterItem **counter)
{
  g_assert(self && (self->live_mask & (1 << type)) && &self->counters[type] == (*counter));
  g_assert(self->use_count > 0);

  self->use_count--;
  *counter = NULL;
}

static gchar *
_stats_build_query_key(StatsCluster *self)
{
  GString *key = g_string_new("");
  gchar buffer[64] = {0};
  const gchar *component_name = stats_cluster_get_component_name(self, buffer, sizeof(buffer));

  g_string_append(key, component_name);

  if (self->id && self->id[0])
    {
      g_string_append_printf(key, ".%s", self->id);
    }
  if (self->instance && self->instance[0])
    {
      g_string_append_printf(key, ".%s", self->instance);
    }

  return g_string_free(key, FALSE);
}

gboolean
stats_cluster_is_alive(StatsCluster *self, gint type)
{
  return ((1<<type) & self->live_mask);
}

StatsCluster *
stats_cluster_new(gint component, const gchar *id, const gchar *instance)
{
  StatsCluster *self = g_new0(StatsCluster, 1);

  self->component = component;
  self->id = g_strdup(id ? : "");
  self->instance = g_strdup(instance ? : "");
  self->use_count = 0;
  self->query_key = _stats_build_query_key(self);
  return self;
}

void
stats_cluster_free(StatsCluster *self)
{
  g_free(self->id);
  g_free(self->instance);
  g_free(self->query_key);
  g_free(self);
}
