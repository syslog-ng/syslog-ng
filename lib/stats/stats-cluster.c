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
#include "stats/stats-cluster.h"

#include <string.h>

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

const gchar *
stats_cluster_get_type_name(gint type)
{
  static const gchar *tag_names[SC_TYPE_MAX] =
  {
    /* [SC_TYPE_DROPPED]   = */ "dropped",
    /* [SC_TYPE_PROCESSED] = */ "processed",
    /* [SC_TYPE_STORED]   = */  "stored",
    /* [SC_TYPE_SUPPRESSED] = */ "suppressed",
    /* [SC_TYPE_STAMP] = */ "stamp",
  };

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
  if ((self->component & SCS_SOURCE_MASK) == SCS_GROUP)
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
      g_snprintf(buf, buf_len, "%s%s", 
                 _get_component_prefix(self->component),
                 _get_module_name(self->component));
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

StatsCluster *
stats_cluster_new(gint component, const gchar *id, const gchar *instance)
{
  StatsCluster *self = g_new0(StatsCluster, 1);
      
  self->component = component;
  self->id = g_strdup(id);
  self->instance = g_strdup(instance);
  self->use_count = 0;
  return self;
}

void
stats_cluster_free(StatsCluster *self)
{ 
  g_free(self->id);
  g_free(self->instance);
  g_free(self);
}
