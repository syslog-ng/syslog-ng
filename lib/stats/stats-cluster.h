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
#ifndef STATS_CLUSTER_H_INCLUDED
#define STATS_CLUSTER_H_INCLUDED 1

#include "stats/stats-counter.h"

typedef enum
{
  SC_TYPE_MIN,
  SC_TYPE_DROPPED=0, /* number of messages dropped */
  SC_TYPE_PROCESSED, /* number of messages processed */
  SC_TYPE_STORED,    /* number of messages on disk */
  SC_TYPE_SUPPRESSED,/* number of messages suppressed */
  SC_TYPE_STAMP,     /* timestamp */
  SC_TYPE_MAX
} StatsCounterType;

enum
{
  /* direction bits, used to distinguish between source/destination drivers */
  SCS_SOURCE         = 0x0100,
  SCS_DESTINATION    = 0x0200,
  
  /* drivers, this should be registered dynamically */
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
  SCS_MAX,
  SCS_SOURCE_MASK    = 0xff
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
  StatsCounterItem counters[SC_TYPE_MAX];
  guint16 ref_cnt;
  /* syslog-ng component/driver/subsystem that registered this cluster */
  guint16 component;
  gchar *id;
  gchar *instance;
  guint16 live_mask;
  guint16 dynamic:1;
} StatsCluster;

typedef void (*StatsForeachCounterFunc)(StatsCluster *sc, gint type, StatsCounterItem *counter, gpointer user_data);

const gchar *stats_cluster_get_type_name(gint type);
const gchar *stats_cluster_get_component_name(StatsCluster *self, gchar *buf, gsize buf_len);

void stats_cluster_foreach_counter(StatsCluster *self, StatsForeachCounterFunc func, gpointer user_data);

gboolean stats_cluster_equal(const StatsCluster *sc1, const StatsCluster *sc2);
guint stats_cluster_hash(const StatsCluster *self);

StatsCluster *stats_cluster_new(gint component, const gchar *id, const gchar *instance);
void stats_cluster_free(StatsCluster *self);

#endif
