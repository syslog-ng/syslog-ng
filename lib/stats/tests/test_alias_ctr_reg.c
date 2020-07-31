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

TestSuite(stats_alias_counter, .init = app_startup, .fini = app_shutdown);

Test(stats_alias_counter, register_ctr)
{
  StatsCounterItem *counter = NULL;
  StatsCounterItem *alias_counter = NULL;

  stats_lock();
  {
    StatsClusterKey sc_key;
    stats_cluster_logpipe_key_set(&sc_key, SCS_GLOBAL, "test_ctr", NULL);
    stats_register_counter(0, &sc_key, SC_TYPE_PROCESSED, &counter);
    stats_cluster_logpipe_key_set(&sc_key, SCS_GLOBAL, "test_ctr.alias", NULL);
    StatsCluster *sc = stats_register_alias_counter(0, &sc_key, SC_TYPE_PROCESSED, counter);
    cr_assert_not_null(sc);
    alias_counter = stats_cluster_get_counter(sc, SC_TYPE_PROCESSED);
  }
  stats_unlock();

  cr_expect(alias_counter->external);
  cr_expect_eq(&counter->value, alias_counter->value_ref);
  stats_counter_set(counter, 12);
  cr_expect_eq(stats_counter_get(alias_counter), 12);

  stats_lock();
  {
    StatsClusterKey sc_key;
    stats_cluster_logpipe_key_set(&sc_key, SCS_GLOBAL, "test_ctr.alias", NULL);
    stats_unregister_alias_counter(&sc_key, SC_TYPE_PROCESSED, counter);
    stats_counter_dec(counter);
    cr_expect_eq(stats_counter_get(counter), 11);
    StatsCluster *sc = stats_register_alias_counter(0, &sc_key, SC_TYPE_PROCESSED, counter);
    alias_counter = stats_cluster_get_counter(sc, SC_TYPE_PROCESSED);
    cr_expect(alias_counter->external);
    cr_expect_eq(&counter->value, alias_counter->value_ref);
    stats_cluster_logpipe_key_set(&sc_key, SCS_GLOBAL, "test_ctr", NULL);
    stats_unregister_counter(&sc_key, SC_TYPE_PROCESSED, &counter);
    cr_expect_eq(stats_counter_get(alias_counter), 11);
    stats_register_counter(0, &sc_key, SC_TYPE_PROCESSED, &counter);
    stats_counter_inc(counter);
    cr_expect_eq(stats_counter_get(alias_counter), 12);
  }
  stats_unlock();
}

