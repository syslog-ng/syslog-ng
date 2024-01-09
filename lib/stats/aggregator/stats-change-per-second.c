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
#include "timeutils/cache.h"
#include "stats/stats-registry.h"
#include "stats/stats-cluster-single.h"
#include "timeutils/cache.h"
#include "stats/stats-cluster.h"
#include <math.h>
#include "messages.h"

#define HOUR_IN_SEC 3600 /* 60*60 */
#define DAY_IN_SEC 86400 /* 60*60*24 */

typedef struct
{
  StatsCounterItem *output_counter;
  gsize average;
  gsize sum;
  gsize last_count;

  gssize duration; /* if the duration equals -1, thats mean, it count since syslog start */
  gchar *name;
} CPSLogic;
/* CPS == Change Per Second */

typedef struct
{
  StatsAggregator super;
  time_t init_time;
  time_t last_add_time;
  gboolean registered;

  StatsCluster *sc_input;
  gint stats_type_input;
  StatsCounterItem *input_counter;

  CPSLogic hour;
  CPSLogic day;
  CPSLogic start;
} StatsAggregatorCPS;


static void
_reset_CPS_logic_values(CPSLogic *self)
{
  /* Both aggregate() and reset() is running in the main thread */

  self->average = 0;
  self->sum = 0;
  self->last_count = 0;
  stats_counter_set(self->output_counter, 0);
}

static void
_track_input_counter(StatsAggregatorCPS *self)
{
  StatsCounterItem *input_counter = stats_cluster_get_counter(self->sc_input, self->stats_type_input);
  self->input_counter = input_counter;
  g_assert(self->input_counter != NULL);

  stats_cluster_track_counter(self->sc_input, self->stats_type_input);
}

static void
_untrack_input_counter(StatsAggregatorCPS *self)
{
  stats_cluster_untrack_counter(self->sc_input, self->stats_type_input, &self->input_counter);
  self->input_counter = NULL;
}

static void
_register_CPS(CPSLogic *self, StatsClusterKey *sc_key, gint level, gint type)
{
  if(!self->output_counter)
    stats_register_counter(level, sc_key, type, &self->output_counter);
}

static void
_register_CPSs(StatsAggregatorCPS *self)
{
  stats_lock();
  StatsClusterKey sc_key;

  self->hour.name = g_strconcat(self->super.key.counter_group_init.counter.name, "_last_1h", NULL);
  stats_cluster_single_key_legacy_set_with_name(&sc_key, self->super.key.legacy.component, self->super.key.legacy.id,
                                                self->super.key.legacy.instance,
                                                self->hour.name);
  _register_CPS(&self->hour, &sc_key, self->super.stats_level, SC_TYPE_SINGLE_VALUE);

  self->day.name = g_strconcat(self->super.key.counter_group_init.counter.name, "_last_24h", NULL);
  stats_cluster_single_key_legacy_set_with_name(&sc_key, self->super.key.legacy.component, self->super.key.legacy.id,
                                                self->super.key.legacy.instance,
                                                self->day.name);
  _register_CPS(&self->day, &sc_key, self->super.stats_level, SC_TYPE_SINGLE_VALUE);

  self->start.name = g_strconcat(self->super.key.counter_group_init.counter.name, "_since_start", NULL);
  stats_cluster_single_key_legacy_set_with_name(&sc_key, self->super.key.legacy.component, self->super.key.legacy.id,
                                                self->super.key.legacy.instance,
                                                self->start.name);
  _register_CPS(&self->start, &sc_key, self->super.stats_level, SC_TYPE_SINGLE_VALUE);

  stats_unlock();
}

static void
_register(StatsAggregator *s)
{
  StatsAggregatorCPS *self = (StatsAggregatorCPS *)s;

  if (self->registered)
    return;

  _register_CPSs(self);
  _track_input_counter(self);
  self->registered = TRUE;
}

static void
_unregister_CPS(CPSLogic *self, StatsClusterKey *sc_key, gint type)
{
  stats_unregister_counter(sc_key, type, &self->output_counter);
  self->output_counter = NULL;
}

static void
_unregister_CPSs(StatsAggregatorCPS *self)
{
  stats_lock();
  StatsClusterKey sc_key;

  stats_cluster_single_key_legacy_set_with_name(&sc_key, self->super.key.legacy.component, self->super.key.legacy.id,
                                                self->super.key.legacy.instance,
                                                self->hour.name);
  _unregister_CPS(&self->hour, &sc_key, SC_TYPE_SINGLE_VALUE);
  g_free(self->hour.name);
  self->hour.name = NULL;

  stats_cluster_single_key_legacy_set_with_name(&sc_key, self->super.key.legacy.component, self->super.key.legacy.id,
                                                self->super.key.legacy.instance,
                                                self->day.name);
  _unregister_CPS(&self->day, &sc_key, SC_TYPE_SINGLE_VALUE);
  g_free(self->day.name);
  self->day.name = NULL;

  stats_cluster_single_key_legacy_set_with_name(&sc_key, self->super.key.legacy.component, self->super.key.legacy.id,
                                                self->super.key.legacy.instance,
                                                self->start.name);
  _unregister_CPS(&self->start, &sc_key, SC_TYPE_SINGLE_VALUE);
  g_free(self->start.name);
  self->start.name = NULL;

  stats_unlock();
}

static void
_unregister(StatsAggregator *s)
{
  StatsAggregatorCPS *self = (StatsAggregatorCPS *)s;

  /* unregister() needs to be idempotent, so we can unregister it only after it reaches 0 CPS */
  if (!self->registered)
    return;

  _unregister_CPSs(self);
  _untrack_input_counter(self);
  self->registered = FALSE;
}

static inline time_t
_calc_sec_between_time(time_t *start, time_t *end)
{
  gdouble diff = difftime(*end, *start);
  return (time_t)diff;
}

static gboolean
_is_less_then_duration(StatsAggregatorCPS *self, CPSLogic *logic, time_t *now)
{
  if (logic->duration == -1)
    return TRUE;

  return _calc_sec_between_time(&self->init_time, now) <= logic->duration;
}

static void
_calc_sum(StatsAggregatorCPS *self, CPSLogic *logic, time_t *now)
{
  gsize count = stats_counter_get(self->input_counter);
  gsize diff = count - logic->last_count;
  logic->last_count = count;

  if (!_is_less_then_duration(self, logic, now))
    {
      diff -= logic->average * _calc_sec_between_time(&self->last_add_time, now);
    }

  logic->sum += diff;
  self->last_add_time = *now;
}

static void
_calc_average(StatsAggregatorCPS *self, CPSLogic *logic, time_t *now)
{
  time_t elapsed_time;
  gsize divisor;

  elapsed_time = _calc_sec_between_time(&self->init_time, now);
  divisor = (_is_less_then_duration(self, logic, now)) ? elapsed_time : logic->duration;
  if (divisor <= 0) divisor = 1;
  gsize sum = logic->sum;

  logic->average = (sum / divisor);

  msg_trace("stats-aggregator-cps",
            evt_tag_printf("name", "%s_%s", self->super.key.legacy.id, logic->name),
            evt_tag_long("sum", sum),
            evt_tag_long("divisor", divisor),
            evt_tag_long("cps", (sum/divisor)),
            evt_tag_long("delta_time_since_start", elapsed_time));
}

static void
_aggregate_CPS_logic(StatsAggregatorCPS *self, CPSLogic *logic, time_t *now)
{
  _calc_sum(self, logic, now);
  _calc_average(self, logic, now);
  stats_counter_set(logic->output_counter, logic->average);
}

static void
_aggregate(StatsAggregator *s)
{
  StatsAggregatorCPS *self = (StatsAggregatorCPS *)s;

  iv_validate_now();
  time_t now = iv_now.tv_sec;
  _aggregate_CPS_logic(self, &self->hour, &now);
  _aggregate_CPS_logic(self, &self->day, &now);
  _aggregate_CPS_logic(self, &self->start, &now);
}

static void
_reset_time(StatsAggregatorCPS *self)
{
  iv_validate_now();
  self->init_time = iv_now.tv_sec;
  self->last_add_time = 0;
}

static void
_reset(StatsAggregator *s)
{
  StatsAggregatorCPS *self = (StatsAggregatorCPS *)s;
  _reset_time(self);
  _reset_CPS_logic_values(&self->hour);
  _reset_CPS_logic_values(&self->day);
  _reset_CPS_logic_values(&self->start);
}

static gboolean
_is_orphaned_CPSLogic(CPSLogic *self)
{
  return stats_counter_get(self->output_counter) == 0;
}

static gboolean
_is_orphaned(StatsAggregator *s)
{
  StatsAggregatorCPS *self = (StatsAggregatorCPS *)s;
  return s->use_count == 0 && _is_orphaned_CPSLogic(&self->day) && _is_orphaned_CPSLogic(&self->hour)
         && _is_orphaned_CPSLogic(&self->start);
}

static void
_set_virtual_function(StatsAggregatorCPS *self )
{
  self->super.aggregate = _aggregate;
  self->super.reset = _reset;
  self->super.is_orphaned = _is_orphaned;
  self->super.register_aggr = _register;
  self->super.unregister_aggr = _unregister;
}

StatsAggregator *
stats_aggregator_cps_new(gint level, StatsClusterKey *sc_key, StatsClusterKey *sc_key_input, gint stats_type)
{
  StatsAggregatorCPS *self = g_new0(StatsAggregatorCPS, 1);
  stats_aggregator_init_instance(&self->super, sc_key, level);
  _set_virtual_function(self);

  stats_lock();
  self->sc_input = stats_get_cluster(sc_key_input);
  g_assert(self->sc_input != NULL);
  stats_unlock();

  self->stats_type_input = stats_type;
  _reset_time(self);

  self->hour.duration = HOUR_IN_SEC;
  self->day.duration = DAY_IN_SEC;
  self->start.duration = -1;
  self->super.timer_period = 60;

  return &self->super;
}
