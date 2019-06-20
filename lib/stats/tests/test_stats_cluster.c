/*
 * Copyright (c) 2013 Balabit
 * Copyright (c) 2013 Balázs Scheidler <bazsi@balabit.hu>
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

#include <criterion/criterion.h>
#include "stats/stats-cluster.h"
#include "stats/stats-cluster-single.h"
#include "apphook.h"

guint SCS_FILE;

static void
setup(void)
{
  app_startup();
  SCS_FILE = stats_register_type("file");
}

Test(stats_cluster, test_stats_cluster_single)
{
  StatsCluster *sc;
  StatsClusterKey sc_key;
  stats_cluster_single_key_set(&sc_key, SCS_GLOBAL, "logmsg_allocated_bytes", NULL);

  sc = stats_cluster_new(&sc_key);
  cr_assert_str_eq(sc->query_key, "global.logmsg_allocated_bytes", "Unexpected query key");
  cr_assert_eq(sc->counter_group.capacity, 1, "Invalid group capacity");
  stats_cluster_free(sc);
}

Test(stats_cluster, test_stats_cluster_new_replaces_NULL_with_an_empty_string)
{
  StatsCluster *sc;
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key, SCS_SOURCE | SCS_FILE, NULL, NULL );

  sc = stats_cluster_new(&sc_key);
  cr_assert_str_eq(sc->key.id, "", "StatsCluster->id is not properly defaulted to an empty string");
  cr_assert_str_eq(sc->key.instance, "", "StatsCluster->instance is not properly defaulted to an empty string");
  stats_cluster_free(sc);
}

static void
assert_stats_cluster_equals(StatsCluster *sc1, StatsCluster *sc2)
{
  cr_assert(stats_cluster_equal(sc1, sc2), "unexpected unequal StatsClusters");
}

static void
assert_stats_cluster_mismatches(StatsCluster *sc1, StatsCluster *sc2)
{
  cr_assert_not(stats_cluster_equal(sc1, sc2), "unexpected equal StatsClusters");
}

static void
assert_stats_cluster_equals_and_free(StatsCluster *sc1, StatsCluster *sc2)
{
  assert_stats_cluster_equals(sc1, sc2);
  stats_cluster_free(sc1);
  stats_cluster_free(sc2);
}

static void
assert_stats_cluster_mismatches_and_free(StatsCluster *sc1, StatsCluster *sc2)
{
  assert_stats_cluster_mismatches(sc1, sc2);
  stats_cluster_free(sc1);
  stats_cluster_free(sc2);
}

Test(stats_cluster, test_stats_cluster_equal_if_component_id_and_instance_are_the_same)
{
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key, SCS_SOURCE | SCS_FILE, "id", "instance" );
  assert_stats_cluster_equals_and_free(stats_cluster_new(&sc_key),
                                       stats_cluster_new(&sc_key));

  stats_cluster_logpipe_key_set(&sc_key, SCS_SOURCE | SCS_FILE, "id", "instance1" );
  StatsClusterKey sc_key2;
  stats_cluster_logpipe_key_set(&sc_key2, SCS_SOURCE | SCS_FILE, "id", "instance2" );
  assert_stats_cluster_mismatches_and_free(stats_cluster_new(&sc_key),
                                           stats_cluster_new(&sc_key2));

  stats_cluster_logpipe_key_set(&sc_key, SCS_SOURCE | SCS_FILE, "id1", "instance" );
  stats_cluster_logpipe_key_set(&sc_key2, SCS_SOURCE | SCS_FILE, "id2", "instance" );
  assert_stats_cluster_mismatches_and_free(stats_cluster_new(&sc_key),
                                           stats_cluster_new(&sc_key2));

  stats_cluster_logpipe_key_set(&sc_key, SCS_SOURCE | SCS_FILE, "id", "instance" );
  stats_cluster_logpipe_key_set(&sc_key2, SCS_DESTINATION | SCS_FILE, "id", "instance" );
  assert_stats_cluster_mismatches_and_free(stats_cluster_new(&sc_key),
                                           stats_cluster_new(&sc_key2));
}

Test(stats_cluster, test_stats_cluster_key_not_equal_when_custom_tags_are_different)
{
  StatsClusterKey sc_key1;
  StatsClusterKey sc_key2;
  stats_cluster_single_key_set_with_name(&sc_key1, SCS_SOURCE | SCS_FILE, "id", "instance", "name1");
  stats_cluster_single_key_set_with_name(&sc_key2, SCS_SOURCE | SCS_FILE, "id", "instance", "name2");
  cr_assert_not(stats_cluster_key_equal(&sc_key1, &sc_key2), "%s", __FUNCTION__);
}

Test(stats_cluster, test_stats_cluster_key_equal_when_custom_tags_are_the_same)
{
  StatsClusterKey sc_key1;
  StatsClusterKey sc_key2;
  stats_cluster_single_key_set_with_name(&sc_key1, SCS_SOURCE | SCS_FILE, "id", "instance", "name");
  stats_cluster_single_key_set_with_name(&sc_key2, SCS_SOURCE | SCS_FILE, "id", "instance", "name");
  cr_assert(stats_cluster_key_equal(&sc_key1, &sc_key2), "%s", __FUNCTION__);
}

typedef struct _ValidateCountersState
{
  gint type1;
  va_list types;
  gint validate_count;
} ValidateCountersState;

static void
_validate_yielded_counters(StatsCluster *sc, gint type, StatsCounterItem *counter, gpointer user_data)
{
  ValidateCountersState *st = (ValidateCountersState *) user_data;
  gint t;

  t = va_arg(st->types, gint);
  cr_assert_geq(t, 0, "foreach counter returned a new counter, but we expected the end already");
  cr_assert_eq(type, t, "Counter type mismatch");
  st->validate_count++;
}

static void
assert_stats_foreach_yielded_counters_matches(StatsCluster *sc, ...)
{
  ValidateCountersState st;
  va_list va;
  gint type_count = 0;
  gint t;

  va_start(va, sc);
  st.validate_count = 0;
  va_copy(st.types, va);

  t = va_arg(va, gint);
  while (t >= 0)
    {
      type_count++;
      t = va_arg(va, gint);
    }

  stats_cluster_foreach_counter(sc, _validate_yielded_counters, &st);
  va_end(va);

  cr_assert_eq(type_count, st.validate_count, "the number of validated counters mismatch the expected size");
}

Test(stats_cluster, test_stats_foreach_counter_yields_tracked_counters)
{
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key, SCS_SOURCE | SCS_FILE, "id", "instance" );
  StatsCluster *sc = stats_cluster_new(&sc_key);

  assert_stats_foreach_yielded_counters_matches(sc, -1);

  stats_cluster_track_counter(sc, SC_TYPE_PROCESSED);
  assert_stats_foreach_yielded_counters_matches(sc, SC_TYPE_PROCESSED, -1);

  stats_cluster_track_counter(sc, SC_TYPE_STAMP);
  assert_stats_foreach_yielded_counters_matches(sc, SC_TYPE_PROCESSED, SC_TYPE_STAMP, -1);
  stats_cluster_free(sc);
}

Test(stats_cluster, test_stats_foreach_counter_never_forgets_untracked_counters)
{
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key, SCS_SOURCE | SCS_FILE, "id", "instance" );
  StatsCluster *sc = stats_cluster_new(&sc_key);
  StatsCounterItem *processed, *stamp;

  processed = stats_cluster_track_counter(sc, SC_TYPE_PROCESSED);
  stamp = stats_cluster_track_counter(sc, SC_TYPE_STAMP);

  stats_cluster_untrack_counter(sc, SC_TYPE_PROCESSED, &processed);
  assert_stats_foreach_yielded_counters_matches(sc, SC_TYPE_PROCESSED, SC_TYPE_STAMP, -1);
  stats_cluster_untrack_counter(sc, SC_TYPE_STAMP, &stamp);
  assert_stats_foreach_yielded_counters_matches(sc, SC_TYPE_PROCESSED, SC_TYPE_STAMP, -1);

  stats_cluster_free(sc);
}

static void
assert_stats_component_name(gint component, const gchar *expected)
{
  gchar buf[32];
  const gchar *name;
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key, component, NULL, NULL );
  StatsCluster *sc = stats_cluster_new(&sc_key);

  name = stats_cluster_get_component_name(sc, buf, sizeof(buf));
  cr_assert_str_eq(name, expected, "component name mismatch");
  stats_cluster_free(sc);
}

Test(stats_cluster, test_get_component_name_translates_component_to_name_properly)
{
  assert_stats_component_name(SCS_SOURCE | SCS_FILE, "src.file");
  assert_stats_component_name(SCS_DESTINATION | SCS_FILE, "dst.file");
  assert_stats_component_name(SCS_GLOBAL, "global");
  assert_stats_component_name(SCS_SOURCE | SCS_GROUP, "source");
  assert_stats_component_name(SCS_DESTINATION | SCS_GROUP, "destination");
}

Test(stats_cluster, test_get_counter)
{
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key, SCS_SOURCE | SCS_FILE, "id", "instance" );
  StatsCluster *sc = stats_cluster_new(&sc_key);
  StatsCounterItem *processed;

  cr_assert_null(stats_cluster_get_counter(sc, SC_TYPE_PROCESSED), "get counter before tracked");
  processed = stats_cluster_track_counter(sc, SC_TYPE_PROCESSED);
  cr_assert_eq(stats_cluster_get_counter(sc, SC_TYPE_PROCESSED), processed, "get counter after tracked");

  StatsCounterItem *saved_processed = processed;
  stats_cluster_untrack_counter(sc, SC_TYPE_PROCESSED, &processed);
  cr_assert_null(processed, "untrack counter");
  cr_assert_eq(stats_cluster_get_counter(sc, SC_TYPE_PROCESSED), saved_processed, "get counter after untracked");
  stats_cluster_free(sc);
}

Test(stats_cluster, test_register_type)
{
  guint first = stats_register_type("HAL");
  guint second = stats_register_type("Just what do you think you are doing, Dave?");
  cr_assert_eq(first + 1, second);

  guint same = stats_register_type("HAL");
  cr_assert_eq(first, same);
}

TestSuite(stats_cluster, .init=setup, .fini = app_shutdown);
