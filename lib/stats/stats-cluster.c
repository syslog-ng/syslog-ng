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

gboolean
stats_cluster_key_labels_equal(StatsClusterLabel *l1, gsize l1_len, StatsClusterLabel *l2, gsize l2_len)
{
  if (l1_len != l2_len)
    return FALSE;

  for (gsize i = 0; i < l1_len; ++i)
    {
      if (strcmp(l1[i].name, l2[i].name) != 0 || g_strcmp0(l1[i].value, l2[i].value) != 0)
        return FALSE;
    }

  return TRUE;
}

guint
stats_cluster_key_labels_hash(StatsClusterLabel *labels, gsize labels_len)
{
  guint hash = 0;
  for (gsize i = 0; i < labels_len; ++i)
    {
      StatsClusterLabel *label = &labels[i];
      hash += g_str_hash(label->name);
      if (label->value)
        hash += g_str_hash(label->value);
    }

  return hash;
}

StatsClusterLabel *
stats_cluster_key_labels_clone(StatsClusterLabel *labels, gsize labels_len)
{
  StatsClusterLabel *cloned = g_new(StatsClusterLabel, labels_len);

  for (gsize i = 0; i < labels_len; ++i)
    {
      StatsClusterLabel *label = &labels[i];
      g_assert(label->name);

      cloned[i] = stats_cluster_label(g_strdup(label->name), g_strdup(label->value));
    }

  return cloned;
}

void
stats_cluster_key_labels_free(StatsClusterLabel *labels, gsize labels_len)
{
  for (gsize i = 0; i < labels_len; ++i)
    {
      g_free((gchar *) labels[i].name);
      g_free((gchar *) labels[i].value);
    }

  g_free(labels);
}

StatsClusterKey *
stats_cluster_key_clone(StatsClusterKey *dst, const StatsClusterKey *src)
{
  dst->name = g_strdup(src->name);
  dst->labels = stats_cluster_key_labels_clone(src->labels, src->labels_len);
  dst->labels_len = src->labels_len;

  dst->formatting = src->formatting;

  dst->legacy.id = g_strdup(src->legacy.id ? : "");
  dst->legacy.component = src->legacy.component;
  dst->legacy.instance = g_strdup(src->legacy.instance ? : "");
  dst->legacy.set = src->legacy.set;

  if (src->counter_group_init.clone)
    src->counter_group_init.clone(&dst->counter_group_init, &src->counter_group_init);
  else
    dst->counter_group_init = src->counter_group_init;

  return dst;
}

void
stats_cluster_key_set(StatsClusterKey *self, const gchar *name, StatsClusterLabel *labels, gsize labels_len,
                      StatsCounterGroupInit counter_group_init)
{
  *self = (StatsClusterKey)
  {
    0
  };
  self->name = name;
  self->labels = labels;
  self->labels_len = labels_len;
  self->counter_group_init = counter_group_init;
}

static inline void
_legacy_set(StatsClusterKey *self, guint16 component, const gchar *id, const gchar *instance,
            StatsCounterGroupInit counter_group_init)
{
  self->legacy.id = (id?id:"");
  self->legacy.component = component;
  self->legacy.instance = (instance?instance:"");
  self->legacy.set = 1;
  self->counter_group_init = counter_group_init;
}

void
stats_cluster_key_legacy_set(StatsClusterKey *self, guint16 component, const gchar *id, const gchar *instance,
                             StatsCounterGroupInit counter_group_ctor)
{
  *self = (StatsClusterKey)
  {
    0
  };
  _legacy_set(self, component, id, instance, counter_group_ctor);
}

void
stats_cluster_key_add_legacy_alias(StatsClusterKey *self, guint16 component, const gchar *id, const gchar *instance,
                                   StatsCounterGroupInit counter_group_ctor)
{
  _legacy_set(self, component, id, instance, counter_group_ctor);
}

void
stats_cluster_key_cloned_free(StatsClusterKey *self)
{
  g_free((gchar *)(self->name));
  stats_cluster_key_labels_free(self->labels, self->labels_len);

  g_free((gchar *)(self->legacy.id));
  g_free((gchar *)(self->legacy.instance));

  if (self->counter_group_init.cloned_free)
    self->counter_group_init.cloned_free(&self->counter_group_init);
}

void
stats_cluster_key_free(StatsClusterKey *self)
{
  stats_cluster_key_cloned_free(self);
  g_free(self);
}

void
stats_cluster_foreach_counter(StatsCluster *self, StatsForeachCounterFunc func, gpointer user_data)
{
  gint type;

  for (type = 0; type < self->counter_group.capacity; type++)
    {
      StatsCounterItem *counter = stats_cluster_get_counter(self, type);
      if (counter)
        func(self, type, counter, user_data);
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
  if ((self->key.legacy.component & SCS_SOURCE_MASK) == SCS_GROUP)
    {
      if (self->key.legacy.component & SCS_SOURCE)
        return "source";
      else if (self->key.legacy.component & SCS_DESTINATION)
        return "destination";
      else
        g_assert_not_reached();
    }
  else
    {
      g_snprintf(buf, buf_len, "%s%s",
                 _get_component_prefix(self->key.legacy.component),
                 _get_module_name(self->key.legacy.component));
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

  return (self->init == other->init) && (self->counter.names == other->counter.names);
}

gboolean
stats_cluster_key_equal(const StatsClusterKey *key1, const StatsClusterKey *key2)
{
  if (stats_cluster_key_is_legacy(key1) != stats_cluster_key_is_legacy(key2))
    return FALSE;

  if (stats_cluster_key_is_legacy(key1))
    {
      return key1->legacy.component == key2->legacy.component
             && strcmp(key1->legacy.id, key2->legacy.id) == 0
             && strcmp(key1->legacy.instance, key2->legacy.instance) == 0
             && stats_counter_group_init_equals(&key1->counter_group_init, &key2->counter_group_init);
    }

  return strcmp(key1->name, key2->name) == 0
         && stats_cluster_key_labels_equal(key1->labels, key1->labels_len, key2->labels, key2->labels_len)
         && stats_counter_group_init_equals(&key1->counter_group_init, &key2->counter_group_init);
}

guint
stats_cluster_key_hash(const StatsClusterKey *self)
{
  if (stats_cluster_key_is_legacy(self))
    return g_str_hash(self->legacy.id) + g_str_hash(self->legacy.instance) + self->legacy.component;

  return g_str_hash(self->name) + stats_cluster_key_labels_hash(self->labels, self->labels_len);
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

  if (self->use_count == 0 && (*counter)->external)
    {
      stats_counter_clear(*counter);
      gint type_mask = 1 << type;
      self->live_mask &= ~type_mask;
    }

  *counter = NULL;
}

static inline void
_reset_counter_if_needed(StatsCluster *sc, gint type, StatsCounterItem *counter, gpointer user_data)
{
  if (strcmp(stats_cluster_get_type_name(sc, type), "memory_usage") == 0)
    return;

  switch (type)
    {
    case SC_TYPE_QUEUED:
      return;
    default:
      stats_counter_set(counter, 0);
    }
}

void
stats_cluster_reset_counter_if_needed(StatsCluster *sc, StatsCounterItem *counter)
{
  _reset_counter_if_needed(sc, counter->type, counter, NULL);
}

static gchar *
_stats_build_query_key(StatsCluster *self)
{
  GString *key = g_string_new("");
  gchar buffer[64] = {0};
  const gchar *component_name = stats_cluster_get_component_name(self, buffer, sizeof(buffer));

  g_string_append(key, component_name);

  if (self->key.legacy.id && self->key.legacy.id[0])
    {
      g_string_append_printf(key, ".%s", self->key.legacy.id);
    }
  if (self->key.legacy.instance && self->key.legacy.instance[0])
    {
      g_string_append_printf(key, ".%s", self->key.legacy.instance);
    }

  return g_string_free(key, FALSE);
}

gboolean
stats_cluster_is_alive(StatsCluster *self, gint type)
{
  g_assert(type < self->counter_group.capacity);

  return !!((1<<type) & self->live_mask);
}

StatsCluster *
stats_cluster_new(const StatsClusterKey *key)
{
  StatsCluster *self = g_new0(StatsCluster, 1);

  stats_cluster_key_clone(&self->key, key);
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
  stats_counter_clear(item);
}

void
stats_cluster_free(StatsCluster *self)
{
  stats_cluster_foreach_counter(self, stats_cluster_free_counter, NULL);
  stats_cluster_key_cloned_free(&self->key);
  g_free(self->query_key);
  stats_counter_group_free(&self->counter_group);
  g_free(self);
}
