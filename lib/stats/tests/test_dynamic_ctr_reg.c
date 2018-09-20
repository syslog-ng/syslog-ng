/*
 * Copyright (c) 2017 Balabit
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
#include <criterion/parameterized.h>

#include <limits.h>
#include <time.h>

TestSuite(stats_dynamic_clusters, .init = app_startup, .fini = app_shutdown);

Test(stats_dynamic_clusters, unlimited_by_default)
{
  StatsOptions stats_opts;
  stats_options_defaults(&stats_opts);
  stats_reinit(&stats_opts);
  cr_assert_eq(stats_check_dynamic_clusters_limit(0), TRUE);
  cr_assert_eq(stats_check_dynamic_clusters_limit(UINT_MAX), TRUE);
}

Test(stats_dynamic_clusters, limited)
{
  StatsOptions stats_opts;
  stats_options_defaults(&stats_opts);
  stats_opts.max_dynamic = 2;
  stats_reinit(&stats_opts);
  cr_assert_eq(stats_check_dynamic_clusters_limit(0), TRUE);
  cr_assert_eq(stats_check_dynamic_clusters_limit(1), TRUE);
  cr_assert_eq(stats_check_dynamic_clusters_limit(2), FALSE);
}

Test(stats_dynamic_clusters, register_limited)
{
  StatsOptions stats_opts;
  stats_options_defaults(&stats_opts);
  stats_opts.level = 3;
  stats_opts.max_dynamic = 2;
  stats_reinit(&stats_opts);
  stats_lock();
  {
    StatsClusterKey sc_key;
    stats_cluster_logpipe_key_set(&sc_key, SCS_HOST | SCS_SENDER, NULL, "testhost1");
    StatsCounterItem *counter = NULL;
    StatsCluster *sc = stats_register_dynamic_counter(1, &sc_key, SC_TYPE_PROCESSED, &counter);
    cr_assert_not_null(sc);
    stats_cluster_logpipe_key_set(&sc_key, SCS_HOST | SCS_SENDER, NULL, "testhost2");
    sc = stats_register_dynamic_counter(1, &sc_key, SC_TYPE_PROCESSED, &counter);
    cr_assert_not_null(sc);
    cr_assert_eq(stats_contains_counter(&sc_key, SC_TYPE_PROCESSED), TRUE);
    cr_assert_eq(counter, stats_get_counter(&sc_key, SC_TYPE_PROCESSED));
    stats_cluster_logpipe_key_set(&sc_key, SCS_HOST | SCS_SENDER, NULL, "testhost3");
    sc = stats_register_dynamic_counter(1, &sc_key, SC_TYPE_PROCESSED, &counter);
    cr_assert_null(sc);
    cr_expect_not(stats_contains_counter(&sc_key, SC_TYPE_PROCESSED));
  }
  stats_unlock();
}

