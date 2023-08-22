/*
 * Copyright (c) 2023 Attila Szakacs
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

#ifndef STATS_CLUSTER_KEY_BUILDER_H_INCLUDED
#define STATS_CLUSTER_KEY_BUILDER_H_INCLUDED

#include "syslog-ng.h"
#include "stats-cluster.h"

typedef struct _StatsClusterKeyBuilder StatsClusterKeyBuilder;

StatsClusterKeyBuilder *stats_cluster_key_builder_new(void);
void stats_cluster_key_builder_push(StatsClusterKeyBuilder *self);
void stats_cluster_key_builder_pop(StatsClusterKeyBuilder *self);
void stats_cluster_key_builder_free(StatsClusterKeyBuilder *self);

void stats_cluster_key_builder_set_name(StatsClusterKeyBuilder *self, const gchar *name);
void stats_cluster_key_builder_set_name_prefix(StatsClusterKeyBuilder *self, const gchar *name_prefix);
void stats_cluster_key_builder_set_name_suffix(StatsClusterKeyBuilder *self, const gchar *name_suffix);
void stats_cluster_key_builder_add_label(StatsClusterKeyBuilder *self, const StatsClusterLabel label);
void stats_cluster_key_builder_set_unit(StatsClusterKeyBuilder *self, StatsClusterUnit unit);
void stats_cluster_key_builder_set_frame_of_reference(StatsClusterKeyBuilder *self,
                                                      StatsClusterFrameOfReference frame_of_reference);
void stats_cluster_key_builder_set_legacy_alias(StatsClusterKeyBuilder *self, guint16 component, const gchar *id,
                                                const gchar *instance);
void stats_cluster_key_builder_set_legacy_alias_name(StatsClusterKeyBuilder *self, const gchar *name);

StatsClusterKey *stats_cluster_key_builder_build_single(const StatsClusterKeyBuilder *self);
StatsClusterKey *stats_cluster_key_builder_build_logpipe(const StatsClusterKeyBuilder *self);

/* Compatibility functions for reproducing stats_instance names based on unsorted labels */
void stats_cluster_key_builder_add_legacy_label(StatsClusterKeyBuilder *self, const StatsClusterLabel label);
const gchar *stats_cluster_key_builder_format_legacy_stats_instance(const StatsClusterKeyBuilder *self,
    gchar *buf, gsize buf_size);

#endif
