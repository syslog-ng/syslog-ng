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
#ifndef STATS_COMPAT_H
#define STATS_COMPAT_H

#include "syslog-ng.h"
#include "stats-counter.h"
#include "stats-cluster.h"
#include "stats-cluster-single.h"
#include "stats-registry.h"
#include "atomic-gssize.h"

#include <stdint.h>

/*
 * StatsByteCounter allows representing "big" accumulating byte-based metrics
 * even on 32-bit architectures, where a simple stats-counter would overflow
 * after 4 GiB.
 *
 * When 64-bit counters are available, they will be used.
 * If not, the counters lose precision but are able to represent larger values.
 *
 * The current implementation is not thread-safe, reimplement it using a CAS
 * loop when needed.
 */

typedef enum _StatsByteCounterPrecision
{
  SBCP_KIB,
  SBCP_MIB,
  SBCP_GIB,
} StatsByteCounterPrecision;

typedef struct _StatsByteCounter
{
  StatsCounterItem *counter;

#if STATS_COUNTER_MAX_VALUE < UINT64_MAX
  atomic_gssize counter_cache;
  gsize precision;
#endif
} StatsByteCounter;

static inline void
stats_byte_counter_init(StatsByteCounter *self, StatsClusterKey *key, gint stats_level,
                        StatsByteCounterPrecision min_precision)
{
  *self = (StatsByteCounter)
  {
    0
  };

#if STATS_COUNTER_MAX_VALUE < UINT64_MAX
  StatsClusterUnit unit = SCU_BYTES;

  switch (min_precision)
    {
    case SBCP_KIB:
      self->precision = 1024;
      unit = SCU_KIB;
      break;
    case SBCP_MIB:
      self->precision = 1024 * 1024;
      unit = SCU_MIB;
      break;
    case SBCP_GIB:
      self->precision = 1024 * 1024 * 1024;
      unit = SCU_GIB;
      break;
    default:
      g_assert_not_reached();
    }

  stats_cluster_single_key_add_unit(key, unit);
#endif

  stats_lock();
  stats_register_counter(stats_level, key, SC_TYPE_SINGLE_VALUE, &self->counter);
  stats_unlock();
}

static inline void
stats_byte_counter_deinit(StatsByteCounter *self, StatsClusterKey *key)
{
  stats_lock();
  stats_unregister_counter(key, SC_TYPE_SINGLE_VALUE, &self->counter);
  stats_unlock();
}

/* this is currently NOT thread-safe */
static inline void
stats_byte_counter_add(StatsByteCounter *self, gsize add)
{
#if STATS_COUNTER_MAX_VALUE < UINT64_MAX
  if (!self->counter)
    return;

  self->counter_cache.value += add;

  if (self->counter_cache.value > self->precision)
    {
      stats_counter_add(self->counter, self->counter_cache.value / self->precision);
      self->counter_cache.value %= self->precision;
    }
#else
  stats_counter_add(self->counter, add);
#endif
}

#endif
