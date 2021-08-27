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
  counter_group->counter_names = self->counter.names;
  counter_group->free_fn = _counter_group_free;
}

void
stats_cluster_single_key_set(StatsClusterKey *key, guint16 component, const gchar *id, const gchar *instance)
{
  stats_cluster_key_set(key, component, id, instance, (StatsCounterGroupInit)
  {
    .counter.names = tag_names, .init = _counter_group_init, .equals = NULL
  });
}

static void
_counter_group_with_name_free(StatsCounterGroup *counter_group)
{
  g_free((gchar *)counter_group->counter_names[0]);
  g_free(counter_group->counter_names);
  _counter_group_free(counter_group);
}

static void
_counter_group_init_with_name(StatsCounterGroupInit *self, StatsCounterGroup *counter_group)
{
  counter_group->counters = g_new0(StatsCounterItem, SC_TYPE_SINGLE_MAX);
  counter_group->capacity = SC_TYPE_SINGLE_MAX;

  const gchar **counter_names = g_new0(const gchar *, 1);
  counter_names[0] = g_strdup(self->counter.name);
  counter_group->counter_names = counter_names;

  counter_group->free_fn = _counter_group_with_name_free;
}

static void
_clone_with_name(StatsCounterGroupInit *dst, const StatsCounterGroupInit *src)
{
  dst->counter.name = g_strdup(src->counter.name);

  dst->init = src->init;
  dst->equals = src->equals;
  dst->clone = src->clone;
  dst->cloned_free = src->cloned_free;
}

static void
_cloned_free_with_name(StatsCounterGroupInit *self)
{
  g_free((gchar *)self->counter.name);
}

static gboolean
_group_init_equals(const StatsCounterGroupInit *self, const StatsCounterGroupInit *other)
{
  g_assert(self != NULL && other != NULL && self->counter.name != NULL && other->counter.name != NULL);
  return (self->init == other->init) && (g_strcmp0(self->counter.name, other->counter.name) == 0);
}

void
stats_cluster_single_key_set_with_name(StatsClusterKey *key, guint16 component, const gchar *id, const gchar *instance,
                                       const gchar *name)
{
  stats_cluster_key_set(key, component, id, instance, (StatsCounterGroupInit)
  {
    .counter.name = name, .init = _counter_group_init_with_name, .equals = _group_init_equals,
    .clone = _clone_with_name, .cloned_free = _cloned_free_with_name
  });
}

