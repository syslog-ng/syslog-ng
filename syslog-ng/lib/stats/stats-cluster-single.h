/*
 * Copyright (c) 2017 Balabit
 * Copyright (c) 2017 Laszlo Budai
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

#ifndef STATS_CLUSTER_SINGLE_H_INCLUDED
#define STATS_CLUSTER_SINGLE_H_INCLUDED

#include "syslog-ng.h"

typedef enum
{
  SC_TYPE_SINGLE_VALUE,
  SC_TYPE_SINGLE_MAX
} StatsCounterGroupSingle;

void stats_cluster_single_key_set(StatsClusterKey *key, guint16 component, const gchar *id, const gchar *instance);
void stats_cluster_single_key_set_with_name(StatsClusterKey *key, guint16 component, const gchar *id,
                                            const gchar *instance, const gchar *name);

#endif

