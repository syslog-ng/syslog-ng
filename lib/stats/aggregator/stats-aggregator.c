/*
 * Copyright (c) 2021 One Identity
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

#include "stats/aggregator/stats-aggregator.h"
#include "stats/stats-registry.h"
#include "stats/stats-cluster-single.h"
#include "mainloop.h"


static void
_restart_timer(StatsAggregator *self)
{
  main_loop_assert_main_thread();

  if (self->timer_period < 0)
    return;

  /* NOTE: our timer may still be registered if _restart_timer() is called
   * after _reset(): in this case stats_aggregator_start() will find an
   * orphaned counter whose timer is still running as the 0 value is caused
   * by reset and not by reaching zero due to aggregate.
   *
   * I hate the orphaned lifecycle model implemented in aggregators: we are
   * keeping counters active if they are non-zero (see the _is_orphaned()
   * method below).  I understand it is nicer that the counter is listed as
   * active, but the hoops we needed to jump just for this is insane.  We
   * should have just kept updating those counters even if the related
   * connection is not active at the moment, who would have cared?  */

  if (iv_timer_registered(&self->update_timer))
    return;

  iv_validate_now();
  self->update_timer.expires = iv_now;
  self->update_timer.expires.tv_sec += self->timer_period;

  iv_timer_register(&self->update_timer);
}

static void
_stop_timer(StatsAggregator *self)
{
  main_loop_assert_main_thread();

  if (iv_timer_registered(&self->update_timer))
    iv_timer_unregister(&self->update_timer);
}

static void
_timer_callback(void *cookie)
{
  StatsAggregator *self = (StatsAggregator *) cookie;

  if (!stats_aggregator_is_orphaned(self))
    stats_aggregator_aggregate(self);

  /* we might become orphaned only after the aggregated value reaches 0 */
  if (stats_aggregator_is_orphaned(self))
    stats_aggregator_unregister(self);
  else
    _restart_timer(self);
}

void
stats_aggregator_register(StatsAggregator *self)
{
  if (self->register_aggr)
    self->register_aggr(self);
  _restart_timer(self);
}

void
stats_aggregator_unregister(StatsAggregator *self)
{
  if (self->unregister_aggr)
    self->unregister_aggr(self);
  _stop_timer(self);
}

/* start/stop aggregation */

void
stats_aggregator_start(StatsAggregator *self)
{
  main_loop_assert_main_thread();
  if (!self)
    return;

  if (stats_aggregator_is_orphaned(self))
    stats_aggregator_register(self);

  ++self->use_count;
}

void
stats_aggregator_stop(StatsAggregator *self)
{
  main_loop_assert_main_thread();
  if (!self)
    return;

  if (self->use_count > 0)
    --self->use_count;

  if (self->use_count == 0)
    stats_aggregator_aggregate(self);

  if (stats_aggregator_is_orphaned(self))
    stats_aggregator_unregister(self);
}

static gboolean
_is_orphaned(StatsAggregator *self)
{
  return self->use_count == 0;
}

static void
_register(StatsAggregator *self)
{
  stats_lock();
  stats_register_counter(self->stats_level, &self->key, SC_TYPE_SINGLE_VALUE, &self->output_counter);
  stats_unlock();
}

static void
_unregister(StatsAggregator *self)
{
  stats_lock();
  stats_unregister_counter(&self->key, SC_TYPE_SINGLE_VALUE, &self->output_counter);
  stats_unlock();
}

static void
_set_virtual_functions(StatsAggregator *self)
{
  self->is_orphaned = _is_orphaned;
  self->register_aggr = _register;
  self->unregister_aggr = _unregister;
}

void
stats_aggregator_init_instance(StatsAggregator *self, StatsClusterKey *sc_key, gint stats_level)
{
  self->use_count = 0;
  self->stats_level = stats_level;
  stats_cluster_key_clone(&self->key, sc_key);
  _set_virtual_functions(self);

  IV_TIMER_INIT(&self->update_timer);
  self->update_timer.cookie = self;
  self->update_timer.handler = _timer_callback;
  self->timer_period = -1;
}

void
stats_aggregator_free(StatsAggregator *self)
{
  stats_cluster_key_cloned_free(&self->key);
  if (self && self->free_fn)
    self->free_fn(self);
  g_free(self);
}
