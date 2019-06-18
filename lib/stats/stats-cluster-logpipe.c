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

#include "stats/stats-cluster-logpipe.h"
#include "stats/stats-cluster.h"

static const gchar *tag_names[SC_TYPE_MAX] =
{
  /* [SC_TYPE_DROPPED]   = */ "dropped",
  /* [SC_TYPE_PROCESSED] = */ "processed",
  /* [SC_TYPE_QUEUED]   = */  "queued",
  /* [SC_TYPE_SUPPRESSED] = */ "suppressed",
  /* [SC_TYPE_STAMP] = */ "stamp",
  /* [SC_TYPE_MEMORY_USAGE] = */ "memory_usage",
  /* [SC_TYPE_DISCARDED] = */ "discarded",
  /* [SC_TYPE_MATCHED] = */ "matched",
  /* [SC_TYPE_NOT_MATCHED] = */ "not_matched",
  /* [SC_TYPE_WRITTEN] = */ "written",
};

static void
_counter_group_logpipe_free(StatsCounterGroup *counter_group)
{
  g_free(counter_group->counters);
}

static void
_counter_group_logpipe_init(StatsCounterGroupInit *self, StatsCounterGroup *counter_group)
{
  counter_group->counters = g_new0(StatsCounterItem, SC_TYPE_MAX);
  counter_group->capacity = SC_TYPE_MAX;
  counter_group->counter_names = self->counter_names;
  counter_group->free_fn = _counter_group_logpipe_free;
}

void
stats_cluster_logpipe_key_set(StatsClusterKey *key, guint16 component, guint direction, const gchar *id,
                              const gchar *instance)
{
  // Todo: delete this from the final version: just a manko during development
  g_assert(!(component & SCS_SOURCE));
  g_assert(!(component & SCS_DESTINATION));

  stats_cluster_key_set(key, component, direction, id, instance, (StatsCounterGroupInit)
  {
    tag_names, _counter_group_logpipe_init
  });
}
