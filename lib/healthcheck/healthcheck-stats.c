/*
 * Copyright (c) 2023 László Várady <laszlo.varady93@gmail.com>
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

#include "healthcheck-stats.h"
#include "healthcheck.h"

#include "stats/stats-cluster-single.h"
#include "stats/stats-registry.h"
#include "stats/stats-counter.h"

#include "apphook.h"
#include "cfg.h"
#include "mainloop.h"
#include "timeutils/misc.h"

#include <iv.h>

typedef struct _HealthCheckStats
{
  HealthCheckStatsOptions options;
  struct iv_timer timer;

  StatsCounterItem *io_worker_latency;
  StatsCounterItem *mainloop_io_worker_roundtrip_latency;
} HealthCheckStats;

static HealthCheckStats healthcheck_stats;

static void
healthcheck_stats_update(HealthCheckResult result, gpointer c)
{
  HealthCheckStats *self = (HealthCheckStats *) c;

  stats_counter_set(self->io_worker_latency, result.io_worker_latency);
  stats_counter_set(self->mainloop_io_worker_roundtrip_latency, result.mainloop_io_worker_roundtrip_latency);
}

static void
healthcheck_stats_timer_start(HealthCheckStats *self)
{
  iv_validate_now();
  self->timer.expires = iv_now;
  timespec_add_msec(&self->timer.expires, self->options.freq * 1000);
  iv_timer_register(&self->timer);
}

static void
healthcheck_stats_timer_stop(HealthCheckStats *self)
{
  if (self->timer.handler && iv_timer_registered(&self->timer))
    iv_timer_unregister(&self->timer);
}

static void
healthcheck_stats_run(gpointer c)
{
  HealthCheckStats *self = (HealthCheckStats *) c;

  HealthCheck *hc = healthcheck_new();
  healthcheck_run(hc, healthcheck_stats_update, self);
  healthcheck_unref(hc);

  if (self->options.freq > 0)
    healthcheck_stats_timer_start(self);
}

static void
_register_counters(HealthCheckStats *self)
{
  StatsClusterKey sc_key_io_worker_latency, sc_key_mainloop_iow_rt_latency;
  stats_cluster_single_key_set(&sc_key_io_worker_latency, "io_worker_latency_seconds", NULL, 0);
  stats_cluster_single_key_add_unit(&sc_key_io_worker_latency, SCU_NANOSECONDS);
  stats_cluster_single_key_set(&sc_key_mainloop_iow_rt_latency,
                               "mainloop_io_worker_roundtrip_latency_seconds", NULL, 0);
  stats_cluster_single_key_add_unit(&sc_key_mainloop_iow_rt_latency, SCU_NANOSECONDS);

  stats_lock();
  stats_register_counter(1, &sc_key_io_worker_latency, SC_TYPE_SINGLE_VALUE, &self->io_worker_latency);
  stats_register_counter(1, &sc_key_mainloop_iow_rt_latency, SC_TYPE_SINGLE_VALUE,
                         &self->mainloop_io_worker_roundtrip_latency);
  stats_unlock();
}

static void
_unregister_counters(HealthCheckStats *self)
{
  StatsClusterKey sc_key_io_worker_latency, sc_key_mainloop_iow_rt_latency;
  stats_cluster_single_key_set(&sc_key_io_worker_latency, "io_worker_latency_seconds", NULL, 0);
  stats_cluster_single_key_set(&sc_key_mainloop_iow_rt_latency,
                               "mainloop_io_worker_roundtrip_latency_seconds", NULL, 0);

  stats_lock();
  stats_unregister_counter(&sc_key_io_worker_latency, SC_TYPE_SINGLE_VALUE, &self->io_worker_latency);
  stats_unregister_counter(&sc_key_mainloop_iow_rt_latency, SC_TYPE_SINGLE_VALUE,
                           &self->mainloop_io_worker_roundtrip_latency);
  stats_unlock();
}

void
healthcheck_stats_init(HealthCheckStatsOptions *options)
{
  healthcheck_stats.options = *options;

  _register_counters(&healthcheck_stats);

  healthcheck_stats_timer_stop(&healthcheck_stats);
  IV_TIMER_INIT(&healthcheck_stats.timer);
  healthcheck_stats.timer.handler = healthcheck_stats_run;
  healthcheck_stats.timer.cookie = &healthcheck_stats;

  if (healthcheck_stats.mainloop_io_worker_roundtrip_latency)
    healthcheck_stats_run(&healthcheck_stats);
}

void
healthcheck_stats_deinit(void)
{
  healthcheck_stats_timer_stop(&healthcheck_stats);
  _unregister_counters(&healthcheck_stats);
}


static inline void
_init(gint type, gpointer c)
{
  MainLoop *main_loop = main_loop_get_instance();
  GlobalConfig *cfg = main_loop_get_current_config(main_loop);
  if (!cfg)
    return;

  healthcheck_stats_init(&cfg->healthcheck_options);
}

static inline void
_deinit(gint type, gpointer c)
{
  healthcheck_stats_deinit();
}

void
healthcheck_stats_global_init(void)
{
  register_application_hook(AH_CONFIG_CHANGED, _init, NULL, AHM_RUN_REPEAT);
  register_application_hook(AH_CONFIG_STOPPED, _deinit, NULL, AHM_RUN_REPEAT);
}
