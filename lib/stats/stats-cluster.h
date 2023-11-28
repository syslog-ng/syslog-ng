/*
 * Copyright (c) 2002-2017 Balabit
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
#ifndef STATS_CLUSTER_H_INCLUDED
#define STATS_CLUSTER_H_INCLUDED 1

#include "stats/stats-counter.h"
#include "stats/stats-cluster-logpipe.h"

enum
{
  /* direction bits, used to distinguish between source/destination drivers */
  SCS_SOURCE         = 0x0100,
  SCS_DESTINATION    = 0x0200,
  SCS_GROUP          = 1,
  SCS_GLOBAL,
  SCS_CENTER,
  SCS_HOST,
  SCS_SENDER,
  SCS_PROGRAM,
  SCS_SEVERITY,
  SCS_FACILITY,
  SCS_TAG,
  SCS_FILTER,
  SCS_PARSER,
  SCS_SOURCE_MASK    = 0xff
};

typedef enum _StatsClusterUnit
{
  SCU_NONE = 0,

  SCU_SECONDS,
  SCU_MINUTES,
  SCU_HOURS,
  SCU_MILLISECONDS,
  SCU_NANOSECONDS,

  SCU_BYTES,
  SCU_KIB,
  SCU_MIB,
  SCU_GIB,
} StatsClusterUnit;

typedef enum _StatsClusterFrameOfReference
{
  SCFOR_NONE = 0,
  SCFOR_ABSOLUTE,

  /*
   * Only applicable for counters with seconds, minutes or hours unit.
   * Has a 1 second precision.
   * Results in a positive value for timestamps older than the time of query.
   */
  SCFOR_RELATIVE_TO_TIME_OF_QUERY,
} StatsClusterFrameOfReference;

typedef struct _StatsCounterGroup StatsCounterGroup;
typedef struct _StatsCounterGroupInit StatsCounterGroupInit;

struct _StatsCounterGroup
{
  StatsCounterItem *counters;
  const gchar **counter_names;
  guint16 capacity;
  gboolean (*get_type_label)(StatsCounterGroup *self, gint type, StatsClusterLabel *label);
  void (*free_fn)(StatsCounterGroup *self);
};

struct _StatsCounterGroupInit
{
  union
  {
    const gchar **names;
    const gchar *name;
  } counter;
  void (*init)(StatsCounterGroupInit *self, StatsCounterGroup *counter_group);
  gboolean (*equals)(const StatsCounterGroupInit *self, const StatsCounterGroupInit *other);
  void (*clone)(StatsCounterGroupInit *dst, const StatsCounterGroupInit *src);
  void (*cloned_free)(StatsCounterGroupInit *self);
};

gboolean stats_counter_group_init_equals(const StatsCounterGroupInit *self, const StatsCounterGroupInit *other);

void stats_counter_group_free(StatsCounterGroup *self);

struct _StatsClusterLabel
{
  const gchar *name;
  const gchar *value;
};

static inline StatsClusterLabel
stats_cluster_label(const gchar *name, const gchar *value)
{
  return (StatsClusterLabel)
  {
    .name = name, .value = value
  };
}

struct _StatsClusterKey
{
  const gchar *name;
  StatsClusterLabel *labels;
  gsize labels_len;

  struct
  {
    StatsClusterUnit stored_unit;
    StatsClusterFrameOfReference frame_of_reference;
  } formatting;

  struct
  {
    const gchar *id;
    /* syslog-ng component/driver/subsystem that registered this cluster */
    guint16 component;
    const gchar *instance;
    guint set:1;
  } legacy;
  StatsCounterGroupInit counter_group_init;
};

/* NOTE: This struct can only be used by the stats implementation and not by client code. */

/* StatsCluster encapsulates a set of related counters that are registered
 * to the same stats source.  In a lot of cases, the same stats source uses
 * multiple counters, so keeping them at the same location makes sense.
 *
 * This also improves performance for dynamic counters that relate to
 * information found in the log stream.  In that case multiple counters can
 * be registered with a single hash lookup */
typedef struct _StatsCluster
{
  StatsClusterKey key;
  StatsCounterGroup counter_group;
  guint16 use_count;
  guint16 live_mask;
  guint16 dynamic:1;
  gchar *query_key;
} StatsCluster;

typedef void (*StatsForeachCounterFunc)(StatsCluster *sc, gint type, StatsCounterItem *counter, gpointer user_data);

void stats_cluster_init(void);
void stats_cluster_deinit(void);

guint stats_register_type(const gchar *type_name);
const gchar *stats_cluster_get_type_name(StatsCluster *self, gint type);
const gchar *stats_cluster_get_component_name(StatsCluster *self, gchar *buf, gsize buf_len);

void stats_cluster_foreach_counter(StatsCluster *self, StatsForeachCounterFunc func, gpointer user_data);

StatsClusterKey *stats_cluster_key_clone(StatsClusterKey *dst, const StatsClusterKey *src);
void stats_cluster_key_cloned_free(StatsClusterKey *self);
void stats_cluster_key_free(StatsClusterKey *self);
gboolean stats_cluster_key_equal(const StatsClusterKey *key1, const StatsClusterKey *key2);
guint stats_cluster_key_hash(const StatsClusterKey *self);

StatsCounterItem *stats_cluster_track_counter(StatsCluster *self, gint type);
StatsCounterItem *stats_cluster_get_counter(StatsCluster *self, gint type);
void stats_cluster_untrack_counter(StatsCluster *self, gint type, StatsCounterItem **counter);
gboolean stats_cluster_is_alive(StatsCluster *self, gint type);
void stats_cluster_reset_counter_if_needed(StatsCluster *sc, StatsCounterItem *counter);

static inline gboolean
stats_cluster_is_orphaned(StatsCluster *self)
{
  return self->use_count == 0;
}

static inline gboolean
stats_cluster_get_type_label(StatsCluster *self, gint type, StatsClusterLabel *label)
{
  if (!self->counter_group.get_type_label)
    return FALSE;

  return self->counter_group.get_type_label(&self->counter_group, type, label);
}

StatsCluster *stats_cluster_new(const StatsClusterKey *key);
StatsCluster *stats_cluster_dynamic_new(const StatsClusterKey *key);
void stats_cluster_free(StatsCluster *self);

void stats_cluster_key_set(StatsClusterKey *self, const gchar *name, StatsClusterLabel *labels, gsize labels_len,
                           StatsCounterGroupInit counter_group_ctor);
void stats_cluster_key_legacy_set(StatsClusterKey *self, guint16 component, const gchar *id, const gchar *instance,
                                  StatsCounterGroupInit counter_group_ctor);
void stats_cluster_key_add_legacy_alias(StatsClusterKey *self, guint16 component, const gchar *id,
                                        const gchar *instance,
                                        StatsCounterGroupInit counter_group_ctor);

static inline gboolean
stats_cluster_key_is_legacy(const StatsClusterKey *self)
{
  return self->legacy.set;
}

#endif
