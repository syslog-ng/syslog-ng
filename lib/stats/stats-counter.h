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

typedef struct _StatsCounterItem
{
  gint value;
} StatsCounterItem;


static inline void
stats_counter_add(StatsCounterItem *counter, gint add)
{
  if (counter)
    g_atomic_int_add(&counter->value, add);
}

static inline void
stats_counter_inc(StatsCounterItem *counter)
{
  if (counter)
    g_atomic_int_inc(&counter->value);
}

static inline void
stats_counter_dec(StatsCounterItem *counter)
{
  if (counter)
    g_atomic_int_add(&counter->value, -1);
}

/* NOTE: this is _not_ atomic and doesn't have to be as sets would race anyway */
static inline void
stats_counter_set(StatsCounterItem *counter, guint32 value)
{
  if (counter)
    counter->value = value;
}

/* NOTE: this is _not_ atomic and doesn't have to be as sets would race anyway */
static inline guint32
stats_counter_get(StatsCounterItem *counter)
{
  guint32 result = 0;

  if (counter)
    result = counter->value;
  return result;
}

void stats_reset_non_stored_counters(void);

#endif
