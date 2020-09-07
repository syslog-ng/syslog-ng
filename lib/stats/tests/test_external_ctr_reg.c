/*
 * Copyright (c) 2020 One Identity
 * Copyright (c) 2019 Laszlo Budai <laszlo.budai@oneidentity.com>
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


#include "apphook.h"
#include "logmsg/logmsg.h"
#include "stats/stats-cluster.h"
#include "stats/stats-cluster-single.h"
#include "stats/stats-counter.h"
#include "stats/stats-query.h"
#include "stats/stats-registry.h"
#include "syslog-ng.h"

#include <criterion/criterion.h>

#include <limits.h>
#include <time.h>

TestSuite(stats_external_counter, .init = app_startup, .fini = app_shutdown);

Test(stats_external_counter, register_logpipe_cluster_ctr)
{
  atomic_gssize test_ctr;
  atomic_gssize_set(&test_ctr, 11);
  StatsCounterItem *counter = NULL;

  stats_lock();
  {
    StatsClusterKey sc_key;
    stats_cluster_logpipe_key_set(&sc_key, SCS_GLOBAL, "test_ctr", NULL);
    StatsCluster *sc = stats_register_external_counter(0, &sc_key, SC_TYPE_PROCESSED, &test_ctr);
    cr_assert_not_null(sc);
    counter = stats_cluster_get_counter(sc, SC_TYPE_PROCESSED);
  }
  stats_unlock();

  cr_expect_eq(atomic_gssize_get(&test_ctr), 11);
  cr_expect_eq(&test_ctr, counter->value_ref);
  cr_expect_eq(stats_counter_get(counter), 11);
}

static StatsCounterItem *
_register_external_stats_counter(atomic_gssize *ctr, gssize initial_value)
{
  StatsCounterItem *counter = NULL;
  atomic_gssize_set(ctr, initial_value);

  stats_lock();
  {
    StatsClusterKey sc_key;
    stats_cluster_logpipe_key_set(&sc_key, SCS_GLOBAL, "test_ctr", NULL);
    StatsCluster *sc = stats_register_external_counter(0, &sc_key, SC_TYPE_PROCESSED, ctr);
    counter = stats_cluster_get_counter(sc, SC_TYPE_PROCESSED);
    cr_assert_not_null(sc);
  }
  stats_unlock();

  return counter;
}

Test(stats_external_counter, external_ctr_is_read_only_for_stats_set)
{
  atomic_gssize test_ctr;
  StatsCounterItem *stats_ctr = _register_external_stats_counter(&test_ctr, 11);
  stats_counter_set(stats_ctr, 1);
  cr_expect_eq(stats_counter_get(stats_ctr), 11);
};

Test(stats_external_counter, external_ctr_is_read_only_for_stats_inc, .signal=SIGABRT)
{
  atomic_gssize test_ctr;
  StatsCounterItem *stats_ctr = _register_external_stats_counter(&test_ctr, 11);
  stats_counter_inc(stats_ctr);
};

Test(stats_external_counter, external_ctr_is_read_only_for_stats_dec, .signal=SIGABRT)
{
  atomic_gssize test_ctr;
  StatsCounterItem *stats_ctr = _register_external_stats_counter(&test_ctr, 11);
  stats_counter_dec(stats_ctr);
};

Test(stats_external_counter, external_ctr_is_read_only_for_stats_add, .signal=SIGABRT)
{
  atomic_gssize test_ctr;
  StatsCounterItem *stats_ctr = _register_external_stats_counter(&test_ctr, 11);
  stats_counter_add(stats_ctr, 1);
};

Test(stats_external_counter, external_ctr_is_read_only_for_stats_sub, .signal=SIGABRT)
{
  atomic_gssize test_ctr;
  StatsCounterItem *stats_ctr = _register_external_stats_counter(&test_ctr, 11);
  stats_counter_sub(stats_ctr, 1);
};

Test(stats_external_counter, reset_counter_is_disabled_for_external_counters)
{
  atomic_gssize test_ctr;
  atomic_gssize_set(&test_ctr, 11);
  StatsCounterItem *counter = NULL;

  stats_lock();
  {
    StatsClusterKey sc_key;
    stats_cluster_logpipe_key_set(&sc_key, SCS_GLOBAL, "test_ctr", NULL);
    StatsCluster *sc = stats_register_external_counter(0, &sc_key, SC_TYPE_PROCESSED, &test_ctr);
    counter = stats_cluster_get_counter(sc, SC_TYPE_PROCESSED);
    cr_expect_eq(&sc->counter_group.counters[SC_TYPE_PROCESSED], counter);
    stats_unregister_counter(&sc_key, SC_TYPE_PROCESSED, &counter);
    cr_expect_null(counter);
    cr_expect_neq(sc->counter_group.counters[SC_TYPE_PROCESSED].value_ref, &test_ctr);
    atomic_gssize *embedded_ctr = &(sc->counter_group.counters[SC_TYPE_PROCESSED].value);
    cr_expect_eq(atomic_gssize_get(embedded_ctr), 0);
    cr_expect_eq(atomic_gssize_get(&test_ctr), 11);
    stats_register_external_counter(0, &sc_key, SC_TYPE_PROCESSED, &test_ctr);
    counter = stats_get_counter(&sc_key, SC_TYPE_PROCESSED);
    cr_expect_eq(counter->value_ref, &test_ctr);
    stats_register_external_counter(0, &sc_key, SC_TYPE_PROCESSED, &test_ctr);
    counter = stats_get_counter(&sc_key, SC_TYPE_PROCESSED);
    cr_expect_eq(&test_ctr, counter->value_ref);
  }
  stats_unlock();
}

Test(stats_external_counter, register_same_ctr_as_internal_after_external_unregistered)
{
  atomic_gssize test_ctr;
  atomic_gssize_set(&test_ctr, 11);
  StatsCounterItem *counter = NULL;

  stats_lock();
  {
    StatsClusterKey sc_key;
    stats_cluster_logpipe_key_set(&sc_key, SCS_GLOBAL, "test_ctr", NULL);
    stats_register_external_counter(0, &sc_key, SC_TYPE_PROCESSED, &test_ctr);
    counter = stats_get_counter(&sc_key, SC_TYPE_PROCESSED);
    stats_unregister_counter(&sc_key, SC_TYPE_PROCESSED, &counter);
    stats_register_counter(0, &sc_key, SC_TYPE_PROCESSED, &counter);
    cr_expect_neq(counter->value_ref, &test_ctr);
    stats_counter_inc(counter);
    cr_expect_eq(stats_counter_get(counter), 1);
  }
  stats_unlock();
}

Test(stats_external_counter, register_same_ctr_as_external_after_internal_unregistered, .signal = SIGABRT)
{
  atomic_gssize test_ctr;
  atomic_gssize_set(&test_ctr, 11);
  StatsCounterItem *counter = NULL;

  stats_lock();
  {
    StatsClusterKey sc_key;
    stats_cluster_logpipe_key_set(&sc_key, SCS_GLOBAL, "test_ctr", NULL);
    stats_register_counter(0, &sc_key, SC_TYPE_PROCESSED, &counter);
    stats_unregister_counter(&sc_key, SC_TYPE_PROCESSED, &counter);
    // assert, SIGABRT:
    stats_register_external_counter(0, &sc_key, SC_TYPE_PROCESSED, &test_ctr);
    // this is because we are not unset the live mask even when the use_ctr is 0...
    // I'm not sure if it is correct, but we have other unit tests, where we expect the same behaviour
    /*  counter = stats_get_counter(&sc_key, SC_TYPE_PROCESSED);
        cr_expect_eq(counter->value_ref, &test_ctr);
        stats_counter_inc(counter);
        cr_expect_eq(stats_counter_get(counter), 11);
        atomic_gssize_inc(&test_ctr);
        cr_expect_eq(stats_counter_get(counter), 12);*/
  }
  stats_unlock();
}

Test(stats_external_counter, re_register_internal_ctr_as_external, .signal = SIGABRT)
{
  atomic_gssize test_ctr;
  atomic_gssize_set(&test_ctr, 11);
  StatsCounterItem *internal_counter = NULL;

  stats_lock();
  {
    StatsClusterKey sc_key;
    stats_cluster_logpipe_key_set(&sc_key, SCS_GLOBAL, "test_ctr", NULL);
    stats_register_counter(0, &sc_key, SC_TYPE_PROCESSED, &internal_counter);
    stats_unregister_counter(&sc_key, SC_TYPE_PROCESSED, &internal_counter);
    cr_expect_null(internal_counter);
    // assert, SIGABRT:
    stats_register_external_counter(0, &sc_key, SC_TYPE_PROCESSED, &test_ctr);
  }
  stats_unlock();
}

Test(stats_external_counter, re_register_external_ctr_as_internal)
{
  atomic_gssize test_ctr;
  atomic_gssize_set(&test_ctr, 11);
  StatsCounterItem *external_counter = NULL,
                    *internal_counter = NULL,
                     *tmp_counter = NULL;

  stats_lock();
  {
    StatsClusterKey sc_key;
    stats_cluster_logpipe_key_set(&sc_key, SCS_GLOBAL, "test_ctr", "counter");
    stats_register_external_counter(0, &sc_key, SC_TYPE_PROCESSED, &test_ctr);
    tmp_counter = external_counter = stats_get_counter(&sc_key, SC_TYPE_PROCESSED);
    stats_register_counter(0, &sc_key, SC_TYPE_PROCESSED, &internal_counter);
    cr_expect_eq(internal_counter, external_counter);
    stats_unregister_external_counter(&sc_key, SC_TYPE_PROCESSED, &test_ctr);
    external_counter = stats_get_counter(&sc_key, SC_TYPE_PROCESSED);
    cr_expect_not_null(external_counter);
    stats_register_counter(0, &sc_key, SC_TYPE_PROCESSED, &internal_counter);
    stats_unregister_counter(&sc_key, SC_TYPE_PROCESSED, &internal_counter);
    stats_unregister_counter(&sc_key, SC_TYPE_PROCESSED, &tmp_counter);
    stats_register_counter(0, &sc_key, SC_TYPE_PROCESSED, &internal_counter);
    stats_counter_inc(internal_counter);
    cr_expect_eq(stats_counter_get(internal_counter), 1);
  }
  stats_unlock();
}

Test(stats_external_counter, re_register_external_ctr)
{
  atomic_gssize test_ctr;
  atomic_gssize_set(&test_ctr, 11);
  StatsCounterItem *counter1 = NULL,
                    *counter2 = NULL;

  stats_lock();
  {
    StatsClusterKey sc_key;
    stats_cluster_logpipe_key_set(&sc_key, SCS_GLOBAL, "test_ctr", NULL);
    stats_register_external_counter(0, &sc_key, SC_TYPE_PROCESSED, &test_ctr);
    stats_register_external_counter(0, &sc_key, SC_TYPE_PROCESSED, &test_ctr);
    counter1 = stats_get_counter(&sc_key, SC_TYPE_PROCESSED);
    counter2 = stats_get_counter(&sc_key, SC_TYPE_PROCESSED);
    cr_expect_eq(counter1, counter2);
    cr_expect_eq(counter1->value_ref, &test_ctr);
  }
  stats_unlock();
}

