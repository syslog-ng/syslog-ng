/*
 * Copyright (c) 2017 Balabit
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

#ifndef STATS_QUERY_H_INCLUDED
#define STATS_QUERY_H_INCLUDED

#include "stats-cluster.h"
#include "syslog-ng.h"

typedef gboolean (*StatsFormatCb)(StatsCounterItem *ctr, gpointer user_data);
typedef gboolean (*StatsSumFormatCb)(gpointer user_data);
typedef void (*AggregatedMetricsCb)(GList *counters, StatsCounterItem **result);

gboolean stats_query_list(const gchar *expr, StatsFormatCb format_cb, gpointer result);
gboolean stats_query_list_and_reset_counters(const gchar *expr, StatsFormatCb format_cb, gpointer result);
gboolean stats_query_get(const gchar *expr, StatsFormatCb format_cb, gpointer result);
gboolean stats_query_get_and_reset_counters(const gchar *expr, StatsFormatCb format_cb, gpointer result);
gboolean stats_query_get_sum(const gchar *expr, StatsSumFormatCb format_cb, gpointer result);
gboolean stats_query_get_sum_and_reset_counters(const gchar *expr, StatsSumFormatCb format_cb, gpointer result);

void stats_query_init(void);
void stats_query_deinit(void);

void stats_register_view(gchar *name, GList *queries, const AggregatedMetricsCb aggregate);
void stats_query_index_counter(StatsCluster *cluster, gint type);
#endif
