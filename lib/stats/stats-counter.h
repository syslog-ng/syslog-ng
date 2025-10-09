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

#if ! defined(USE_CHECKED_COUNTER_OPS)
# define USE_CHECKED_COUNTER_OPS 1
#endif
#if ! defined(CHECKED_COUNTER_ADD)
# if SYSLOG_NG_ENABLE_DEBUG && USE_CHECKED_COUNTER_OPS
#  define CHECKED_COUNTER_ADD(old_value_expr, add) \
    do { \
        gsize _old = old_value_expr; \
        gsize _add = add; \
        g_assert(_old + _add >= _old && "stats counter add overflow!"); \
    } while (0)
# else
#  define CHECKED_COUNTER_ADD(old_value_expr, add) ((void)(old_value_expr))
# endif
#endif

#if ! defined(CHECKED_COUNTER_SUB)
# if SYSLOG_NG_ENABLE_DEBUG && USE_CHECKED_COUNTER_OPS
#  define CHECKED_COUNTER_SUB(old_value_expr, sub) \
    do { \
        gsize _old = old_value_expr; \
        gsize _sub = sub; \
        g_assert(_old - _sub <= _old && "stats counter sub underflow!"); \
    } while (0)
# else
#  define CHECKED_COUNTER_SUB(old_value_expr, sub) ((void)(old_value_expr))
# endif
#endif

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
      CHECKED_COUNTER_ADD(atomic_gssize_add(&counter->value, add), add);
    }
}

static inline void
stats_counter_sub(StatsCounterItem *counter, gssize sub)
{
  if (counter)
    {
      g_assert(!stats_counter_read_only(counter));
      CHECKED_COUNTER_SUB(atomic_gssize_sub(&counter->value, sub), sub);
    }
}

static inline void
stats_counter_inc(StatsCounterItem *counter)
{
  if (counter)
    {
      g_assert(!stats_counter_read_only(counter));
      CHECKED_COUNTER_ADD(atomic_gssize_inc(&counter->value), 1);
    }
}

static inline void
stats_counter_dec(StatsCounterItem *counter)
{
  if (counter)
    {
      g_assert(!stats_counter_read_only(counter));
      CHECKED_COUNTER_SUB(atomic_gssize_dec(&counter->value), 1);
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
stats_counter_clear(StatsCounterItem *counter)
{
  g_free(counter->name);
  memset(counter, 0, sizeof(*counter));
}

#endif
