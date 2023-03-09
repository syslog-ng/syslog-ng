/*
 * Copyright (c) 2023 László Várady
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
#ifndef STATS_PROMETHEUS_H_INCLUDED
#define STATS_PROMETHEUS_H_INCLUDED 1

#include "syslog-ng.h"
#include "stats-cluster.h"

#define PROMETHEUS_METRIC_PREFIX "syslogng_"

typedef void (*StatsPrometheusRecordFunc)(const char *record, gpointer user_data);

GString *stats_prometheus_format_counter(StatsCluster *sc, gint type, StatsCounterItem *counter);

void stats_generate_prometheus(StatsPrometheusRecordFunc process_record, gpointer user_data, gboolean with_legacy,
                               gboolean *cancelled);

#endif
