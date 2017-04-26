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

static StatsClusterKey*
_clone_stats_cluster_key(StatsClusterKey *dst, const StatsClusterKey *src)
{
  dst->component = src->component;
  dst->id = g_strdup(src->id ? : "");
  dst->instance = g_strdup(src->instance ? : "");

  return dst;
}

void 
stats_cluster_key_set(StatsClusterKey *self, guint16 component, const gchar *id, const gchar *instance)
{
  self->component = component;
  self->id = (id?id:""); 
  self->instance = (instance?instance:"");
}

static void
_stats_cluster_key_cloned_free(StatsClusterKey *self)
{
  g_free((gchar *)(self->id));
  g_free((gchar *)(self->instance));
}

void
stats_cluster_foreach_counter(StatsCluster *self, StatsForeachCounterFunc func, gpointer user_data)
{
  gint type;

  for (type = 0; type < self->counter_group.capacity; type++)
    {
      if (self->live_mask & (1 << type))
        {
          func(self, type, &self->counter_group.counters[type], user_data);
        }
    }
}

const gchar *
stats_cluster_get_type_name(StatsCluster *self, gint type)
{
  return self->counter_group.counter_names[type];
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
  if ((self->key.component & SCS_SOURCE_MASK) == SCS_GROUP)
    {
      if (self->key.component & SCS_SOURCE)
        return "source";
      else if (self->key.component & SCS_DESTINATION)
        return "destination";
      else
        g_assert_not_reached();
    }
  else
    {
      g_snprintf(buf, buf_len, "%s%s",
                 _get_component_prefix(self->key.component),
                 _get_module_name(self->key.component));
      return buf;
    }
}

gboolean
stats_cluster_equal(const StatsCluster *sc1, const StatsCluster *sc2)
{
  return sc1->key.component == sc2->key.component && strcmp(sc1->key.id, sc2->key.id) == 0 && strcmp(sc1->key.instance, sc2->key.instance) == 0;
}

guint
stats_cluster_hash(const StatsCluster *self)
{
  return g_str_hash(self->key.id) + g_str_hash(self->key.instance) + self->key.component;
}

StatsCounterItem *
stats_cluster_track_counter(StatsCluster *self, gint type)
{
  gint type_mask = 1 << type;

  g_assert(type < self->counter_group.capacity);

  self->live_mask |= type_mask;
  self->use_count++;
  return &self->counter_group.counters[type];
}

void
stats_cluster_untrack_counter(StatsCluster *self, gint type, StatsCounterItem **counter)
{
  g_assert(self && (self->live_mask & (1 << type)) && &self->counter_group.counters[type] == (*counter));
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

  if (self->key.id && self->key.id[0])
    {
      g_string_append_printf(key, ".%s", self->key.id);
    }
  if (self->key.instance && self->key.instance[0])
    {
      g_string_append_printf(key, ".%s", self->key.instance);
    }

  return g_string_free(key, FALSE);
}

gboolean
stats_cluster_is_alive(StatsCluster *self, gint type)
{
  return ((1<<type) & self->live_mask);
}

StatsCluster *
stats_cluster_new(StatsClusterKey *key, StatsCounterGroup *group)
{
  StatsCluster *self = g_new0(StatsCluster, 1);

  _clone_stats_cluster_key(&self->key, key);
  self->use_count = 0;
  self->query_key = _stats_build_query_key(self);
  self->counter_group = *group;
  g_assert(self->counter_group.capacity <= sizeof(self->live_mask)*8);
  return self;
}

void
stats_cluster_free(StatsCluster *self)
{
  _stats_cluster_key_cloned_free(&self->key);
  g_free(self->query_key);
  g_free(self->counter_group.counters); //TODO: dtor?
  g_free(self);
}
