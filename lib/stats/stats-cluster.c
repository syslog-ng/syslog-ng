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
#include "mainloop.h"

#include <assert.h>
#include <string.h>

GPtrArray *stats_types;

void
stats_cluster_init(void)
{
  g_assert(!stats_types);
  stats_types = g_ptr_array_new_with_free_func(g_free);

  g_assert(stats_register_type("none") == 0);
  g_assert(stats_register_type("group") == SCS_GROUP);
  g_assert(stats_register_type("global") == SCS_GLOBAL);
  g_assert(stats_register_type("center") == SCS_CENTER);
  g_assert(stats_register_type("host") == SCS_HOST);
  g_assert(stats_register_type("sender") == SCS_SENDER);
  g_assert(stats_register_type("program") == SCS_PROGRAM);
  g_assert(stats_register_type("severity") == SCS_SEVERITY);
  g_assert(stats_register_type("facility") == SCS_FACILITY);
  g_assert(stats_register_type("tag") == SCS_TAG);
  g_assert(stats_register_type("filter") == SCS_FILTER);
  g_assert(stats_register_type("parser") == SCS_PARSER);
}

gboolean
_types_equal(const gchar *a, const gchar *b)
{
  return !strcmp(a, b);
};

guint
stats_register_type(const gchar *type_name)
{
  main_loop_assert_main_thread();
  guint index_ = 0;
  gboolean result = g_ptr_array_find_with_equal_func(stats_types, type_name, (GEqualFunc)_types_equal, &index_);
  if (result)
    return index_;

  g_ptr_array_add(stats_types, g_strdup(type_name));

  guint registered_number = stats_types->len - 1;
  g_assert(registered_number <= SCS_SOURCE_MASK);
  return registered_number;
};

void
stats_cluster_deinit(void)
{
  g_ptr_array_free(stats_types, TRUE);
  stats_types = NULL;
}

static StatsClusterKey *
_clone_stats_cluster_key(StatsClusterKey *dst, const StatsClusterKey *src)
{
  dst->component = src->component;
  dst->id = g_strdup(src->id ? : "");
  dst->instance = g_strdup(src->instance ? : "");
  dst->counter_group_init = src->counter_group_init;

  return dst;
}

void
stats_cluster_key_set(StatsClusterKey *self, guint16 component, const gchar *id, const gchar *instance,
                      StatsCounterGroupInit counter_group_init)
{
  self->component = component;
  self->id = (id?id:"");
  self->instance = (instance?instance:"");
  self->counter_group_init = counter_group_init;
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
  if (type < self->counter_group.capacity)
    return self->counter_group.counter_names[type];

  return "";
}

static const gchar *
_get_module_name(gint source)
{
  gint type = source & SCS_SOURCE_MASK;
  g_assert(type < stats_types->len);
  return g_ptr_array_index(stats_types, type);
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
stats_counter_group_init_equals(const StatsCounterGroupInit *self, const StatsCounterGroupInit *other)
{
  if (!self || !other)
    return FALSE;

  if (self == other)
    return TRUE;

  if (self->equals)
    return self->equals(self, other);

  return (self->init == other->init) && (self->counter_names == other->counter_names);
}

gboolean
stats_cluster_key_equal(const StatsClusterKey *key1, const StatsClusterKey *key2)
{
  return key1->component == key2->component
         && strcmp(key1->id, key2->id) == 0
         && strcmp(key1->instance, key2->instance) == 0
         && stats_counter_group_init_equals(&key1->counter_group_init, &key2->counter_group_init);
}

gboolean
stats_cluster_equal(const StatsCluster *sc1, const StatsCluster *sc2)
{
  return stats_cluster_key_equal(&sc1->key, &sc2->key);
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

StatsCounterItem *
stats_cluster_get_counter(StatsCluster *self, gint type)
{
  gint type_mask = 1 << type;

  g_assert(type < self->counter_group.capacity);

  if (!(self->live_mask & type_mask))
    return NULL;

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
  g_assert(type < self->counter_group.capacity);

  return !!((1<<type) & self->live_mask);
}

gboolean
stats_cluster_is_indexed(StatsCluster *self, gint type)
{
  g_assert(type < self->counter_group.capacity);

  return !!((1<<type) & self->indexed_mask);
}

StatsCluster *
stats_cluster_new(const StatsClusterKey *key)
{
  StatsCluster *self = g_new0(StatsCluster, 1);

  _clone_stats_cluster_key(&self->key, key);
  self->use_count = 0;
  self->query_key = _stats_build_query_key(self);
  key->counter_group_init.init(&self->key.counter_group_init, &self->counter_group);
  g_assert(self->counter_group.capacity <= sizeof(self->live_mask)*8);
  return self;
}

StatsCluster *
stats_cluster_dynamic_new(const StatsClusterKey *key)
{
  StatsCluster *sc = stats_cluster_new(key);
  sc->dynamic = TRUE;

  return sc;
}

void
stats_counter_group_free(StatsCounterGroup *self)
{
  if (self->free_fn)
    self->free_fn(self);
}


static void
stats_cluster_free_counter(StatsCluster *self, gint type, StatsCounterItem *item, gpointer user_data)
{
  stats_counter_free(item);
}

void
stats_cluster_free(StatsCluster *self)
{
  stats_cluster_foreach_counter(self, stats_cluster_free_counter, NULL);
  _stats_cluster_key_cloned_free(&self->key);
  g_free(self->query_key);
  stats_counter_group_free(&self->counter_group);
  g_free(self);
}
