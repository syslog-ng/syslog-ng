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
#ifndef STATS_REGISTRY_H_INCLUDED
#define STATS_REGISTRY_H_INCLUDED 1

#include "stats/stats.h"
#include "stats/stats-cluster.h"

typedef void (*StatsForeachClusterFunc)(StatsCluster *sc, gpointer user_data);
typedef gboolean (*StatsForeachClusterRemoveFunc)(StatsCluster *sc, gpointer user_data);

void stats_lock(void);
void stats_unlock(void);
gboolean stats_check_level(gint level);
void stats_register_counter(gint level, gint component, const gchar *id, const gchar *instance, StatsCounterType type, StatsCounterItem **counter);
StatsCluster *stats_register_dynamic_counter(gint stats_level, gint component, const gchar *id, const gchar *instance, StatsCounterType type, StatsCounterItem **counter, gboolean *new);
void stats_register_and_increment_dynamic_counter(gint stats_level, gint component, const gchar *id, const gchar *instance, time_t timestamp);
void stats_register_associated_counter(StatsCluster *handle, StatsCounterType type, StatsCounterItem **counter);
void stats_unregister_counter(gint component, const gchar *id, const gchar *instance, StatsCounterType type, StatsCounterItem **counter);
void stats_unregister_dynamic_counter(StatsCluster *handle, StatsCounterType type, StatsCounterItem **counter);

void stats_foreach_counter(StatsForeachCounterFunc func, gpointer user_data);
void stats_foreach_cluster(StatsForeachClusterFunc func, gpointer user_data);
void stats_foreach_cluster_remove(StatsForeachClusterRemoveFunc func, gpointer user_data);

void stats_registry_init(void);
void stats_registry_deinit(void);

#endif
