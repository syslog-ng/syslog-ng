/*
 * Copyright (c) 2002-2017 Balabit
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

#ifndef STATS_CLUSTER_LOGPIPE_H_INCLUDED
#define STATS_CLUSTER_LOGPIPE_H_INCLUDED

#include "syslog-ng.h"

typedef enum
{
  SC_TYPE_DROPPED=0, /* number of messages dropped */
  SC_TYPE_PROCESSED, /* number of messages processed */
  SC_TYPE_QUEUED,    /* number of messages on disk */
  SC_TYPE_SUPPRESSED,/* number of messages suppressed */
  SC_TYPE_STAMP,     /* timestamp */
  SC_TYPE_MEMORY_USAGE,
  SC_TYPE_DISCARDED, /* discarded messages of filter */
  SC_TYPE_MATCHED, /* discarded messages of filter */
  SC_TYPE_NOT_MATCHED, /* discarded messages of filter */
  SC_TYPE_WRITTEN, /* number of sent messages */
  SC_TYPE_MAX
} StatsCounterGroupLogPipe;

void stats_cluster_logpipe_key_set(StatsClusterKey *key, guint16 component, guint direction, const gchar *id,
                                   const gchar *instance);

#endif
