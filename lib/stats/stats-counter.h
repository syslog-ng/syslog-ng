/*
 * Copyright (c) 2002-2013 Balabit
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
#ifndef STATS_COUNTER_H_INCLUDED
#define STATS_COUNTER_H_INCLUDED 1

#include "syslog-ng.h"
#include "atomic-gssize.h"

#define STATS_COUNTER_MAX_VALUE G_MAXSIZE

typedef struct _StatsCounterItem
{
  union
  {
    atomic_gssize value;
    atomic_gssize *value_ref;
  };
  gchar *name;
  gint type;
  gboolean external;
} StatsCounterItem;


static gboolean
stats_counter_read_only(StatsCounterItem *counter)
{
  return counter->external;
}

static inline void
stats_counter_add(StatsCounterItem *counter, gssize add)
{
  if (counter)
    {
      g_assert(!stats_counter_read_only(counter));
      atomic_gssize_add(&counter->value, add);
    }
}

static inline void
stats_counter_sub(StatsCounterItem *counter, gssize sub)
{
  if (counter)
    {
      g_assert(!stats_counter_read_only(counter));
      atomic_gssize_sub(&counter->value, sub);
    }
}

static inline void
stats_counter_inc(StatsCounterItem *counter)
{
  if (counter)
    {
      g_assert(!stats_counter_read_only(counter));
      atomic_gssize_inc(&counter->value);
    }
}

static inline void
stats_counter_dec(StatsCounterItem *counter)
{
  if (counter)
    {
      g_assert(!stats_counter_read_only(counter));
      atomic_gssize_dec(&counter->value);
    }
}

static inline void
stats_counter_set(StatsCounterItem *counter, gsize value)
{
  if (counter && !stats_counter_read_only(counter))
    {
      atomic_gssize_set(&counter->value, value);
    }
}

static inline gsize
stats_counter_get(StatsCounterItem *counter)
{
  gsize result = 0;

  if (counter)
    {
      if (!counter->external)
        result = atomic_gssize_get_unsigned(&counter->value);
      else
        result = atomic_gssize_get_unsigned(counter->value_ref);
    }
  return result;
}

/* Can only store positive values. Fixes overflow on 32 bit machines until 2106 if using seconds. */
static inline void
stats_counter_set_time(StatsCounterItem *counter, gint64 value)
{
  stats_counter_set(counter, (gsize) MAX(0, value));
}

static inline gchar *
stats_counter_get_name(StatsCounterItem *counter)
{
  if (counter)
    return counter->name;
  return NULL;
}

static inline void
stats_counter_free(StatsCounterItem *counter)
{
  g_free(counter->name);
}

#endif
