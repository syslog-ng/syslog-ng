/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2012 Balázs Scheidler
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
  
#ifndef STATS_H_INCLUDED
#define STATS_H_INCLUDED

#include "syslog-ng.h"
#include "stats-cluster.h"

typedef struct _StatsOptions
{
  gint log_freq;
  gint level;
  gint lifetime;
} StatsOptions;

enum
{
  STATS_LEVEL0 = 0,
  STATS_LEVEL1,
  STATS_LEVEL2,
  STATS_LEVEL3
};

enum
{
  SCS_SOURCE         = 0x0100,
  SCS_DESTINATION    = 0x0200,
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

const gchar *stats_get_direction_name(gint source);
const gchar *stats_get_source_name(gint source);
const gchar *stats_get_tag_name(gint type);
const gchar *stats_get_direction_and_source_name(gint source, gchar *buf, gsize buf_len);

typedef void (*StatsForeachCounterFunc)(StatsCluster *sc, gint type, StatsCounterItem *counter, gpointer user_data);
typedef void (*StatsForeachClusterFunc)(StatsCluster *sc, gpointer user_data);
typedef gboolean (*StatsForeachClusterRemoveFunc)(StatsCluster *sc, gpointer user_data);

void stats_lock(void);
void stats_unlock(void);
gboolean stats_check_level(gint level);
void stats_register_counter(gint level, gint source, const gchar *id, const gchar *instance, StatsCounterType type, StatsCounterItem **counter);
StatsCluster *stats_register_dynamic_counter(gint stats_level, gint source, const gchar *id, const gchar *instance, StatsCounterType type, StatsCounterItem **counter, gboolean *new);
void stats_register_and_increment_dynamic_counter(gint stats_level, gint source_mask, const gchar *id, const gchar *instance, time_t timestamp);
void stats_register_associated_counter(StatsCluster *handle, StatsCounterType type, StatsCounterItem **counter);
void stats_unregister_counter(gint source, const gchar *id, const gchar *instance, StatsCounterType type, StatsCounterItem **counter);
void stats_unregister_dynamic_counter(StatsCluster *handle, StatsCounterType type, StatsCounterItem **counter);
void stats_foreach_counter(StatsForeachCounterFunc func, gpointer user_data);
void stats_foreach_cluster(StatsForeachClusterFunc func, gpointer user_data);
void stats_foreach_cluster_remove(StatsForeachClusterRemoveFunc func, gpointer user_data);

void stats_reinit(StatsOptions *options);
void stats_init(void);
void stats_destroy(void);

void stats_options_defaults(StatsOptions *options);


#endif

