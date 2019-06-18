/*
 * Copyright (c) 2017 Balabit
 * Copyright (c) 2017 Laszlo Budai
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

#include "stats/stats-cluster-single.h"
#include "stats/stats-cluster.h"

static const gchar *tag_names[SC_TYPE_SINGLE_MAX] =
{
  /* [SC_TYPE_SINGLE_VALUE]   = */ "value",
};

static void
_counter_group_free(StatsCounterGroup *counter_group)
{
  g_free(counter_group->counters);
}

static void
_counter_group_init(StatsCounterGroupInit *self, StatsCounterGroup *counter_group)
{
  counter_group->counters = g_new0(StatsCounterItem, SC_TYPE_SINGLE_MAX);
  counter_group->capacity = SC_TYPE_SINGLE_MAX;
  counter_group->counter_names = self->counter_names;
  counter_group->free_fn = _counter_group_free;
}

void
stats_cluster_single_key_set(StatsClusterKey *key, const gchar *component, guint direction, const gchar *id,
                             const gchar *instance)
{
  stats_cluster_key_set(key, component, direction, id, instance, (StatsCounterGroupInit)
  {
    tag_names,_counter_group_init
  });
}

static void
_counter_group_with_name_free(StatsCounterGroup *counter_group)
{
  g_free(counter_group->counter_names);
  _counter_group_free(counter_group);
}

static void
_counter_group_init_with_name(StatsCounterGroupInit *self, StatsCounterGroup *counter_group)
{
  counter_group->counters = g_new0(StatsCounterItem, SC_TYPE_SINGLE_MAX);
  counter_group->capacity = SC_TYPE_SINGLE_MAX;
  counter_group->counter_names = self->counter_names;
  counter_group->free_fn = _counter_group_with_name_free;
}

static gboolean
_group_init_equals(const StatsCounterGroupInit *self, const StatsCounterGroupInit *other)
{
  g_assert(self != NULL && other != NULL && self->counter_names != NULL && other->counter_names != NULL);
  return (self->init == other->init) && (g_strcmp0(self->counter_names[0], other->counter_names[0]) == 0);
}

void
stats_cluster_single_key_set_with_name(StatsClusterKey *key, const gchar *component, guint direction, const gchar *id,
                                       const gchar *instance,
                                       const gchar *name)
{
  stats_cluster_key_set(key, component, direction, id, instance, (StatsCounterGroupInit)
  {
    tag_names, _counter_group_init_with_name, _group_init_equals
  });
  key->counter_group_init.counter_names = g_new0(const char *, 1);
  key->counter_group_init.counter_names[0] = name;
}
