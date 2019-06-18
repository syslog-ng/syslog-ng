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

  /* drivers, this should be registered dynamically */
  SCS_NONE           = 0,
  SCS_FILE           = 1,
  SCS_PIPE           = 2,
  SCS_TCP            = 3,
  SCS_UDP            = 4,
  SCS_TCP6           = 5,
  SCS_UDP6           = 6,
  SCS_UNIX_STREAM    = 7,
  SCS_UNIX_DGRAM     = 8,
  SCS_SYSLOG         = 9,
  SCS_NETWORK        = 10,
  SCS_INTERNAL       = 11,
  SCS_LOGSTORE       = 12,
  SCS_PROGRAM        = 13,
  SCS_SQL            = 14,
  SCS_SUN_STREAMS    = 15,
  SCS_USERTTY        = 16,
  SCS_GROUP          = 17,
  SCS_CENTER         = 18,
  SCS_HOST           = 19,
  SCS_GLOBAL         = 20,
  SCS_MONGODB        = 21,
  SCS_CLASS          = 22,
  SCS_RULE_ID        = 23,
  SCS_TAG            = 24,
  SCS_SEVERITY       = 25,
  SCS_FACILITY       = 26,
  SCS_SENDER         = 27,
  SCS_SMTP           = 28,
  SCS_AMQP           = 29,
  SCS_STOMP          = 30,
  SCS_REDIS          = 31,
  SCS_SNMP           = 32,
  SCS_RIEMANN        = 33,
  SCS_JOURNALD       = 34,
  SCS_JAVA           = 35,
  SCS_HTTP           = 36,
  SCS_PYTHON         = 37,
  SCS_FILTER         = 38,
  SCS_PARSER         = 39,
  SCS_MONITORING     = 40,
  SCS_STDIN          = 41,
  SCS_OPENBSD        = 42,
  SCS_KAFKA          = 43,
  SCS_MAX,
  SCS_SOURCE_MASK    = 0xff
};

typedef struct _StatsCounterGroup StatsCounterGroup;
typedef struct _StatsCounterGroupInit StatsCounterGroupInit;

struct _StatsCounterGroup
{
  StatsCounterItem *counters;
  const gchar **counter_names;
  guint16 capacity;
  void (*free_fn)(StatsCounterGroup *self);
};

struct _StatsCounterGroupInit
{
  const gchar **counter_names;
  void (*init)(StatsCounterGroupInit *self, StatsCounterGroup *counter_group);
  gboolean (*equals)(const StatsCounterGroupInit *self, const StatsCounterGroupInit *other);
};

gboolean stats_counter_group_init_equals(const StatsCounterGroupInit *self, const StatsCounterGroupInit *other);

void stats_counter_group_free(StatsCounterGroup *self);

struct _StatsClusterKey
{
  /* syslog-ng component/driver/subsystem that registered this cluster */
  const gchar *component;
  guint direction;
  const gchar *id;
  const gchar *instance;
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
  guint16 indexed_mask;
  guint16 dynamic:1;
  gchar *query_key;
} StatsCluster;

typedef void (*StatsForeachCounterFunc)(StatsCluster *sc, gint type, StatsCounterItem *counter, gpointer user_data);

const gchar *stats_cluster_get_type_name(StatsCluster *self, gint type);
const gchar *stats_cluster_get_component_name(StatsCluster *self, gchar *buf, gsize buf_len);

void stats_cluster_foreach_counter(StatsCluster *self, StatsForeachCounterFunc func, gpointer user_data);

gboolean stats_cluster_key_equal(const StatsClusterKey *key1, const StatsClusterKey *key2);
gboolean stats_cluster_equal(const StatsCluster *sc1, const StatsCluster *sc2);
guint stats_cluster_hash(const StatsCluster *self);

StatsCounterItem *stats_cluster_track_counter(StatsCluster *self, gint type);
StatsCounterItem *stats_cluster_get_counter(StatsCluster *self, gint type);
void stats_cluster_untrack_counter(StatsCluster *self, gint type, StatsCounterItem **counter);
gboolean stats_cluster_is_alive(StatsCluster *self, gint type);
gboolean stats_cluster_is_indexed(StatsCluster *self, gint type);

StatsCluster *stats_cluster_new(const StatsClusterKey *key);
StatsCluster *stats_cluster_dynamic_new(const StatsClusterKey *key);
void stats_cluster_free(StatsCluster *self);

void stats_cluster_key_set(StatsClusterKey *self, const gchar *component, guint direction, const gchar *id,
                           const gchar *instance, StatsCounterGroupInit counter_group_ctor);
const gchar *stats_get_module_name(gint source);
#endif
