#include "testutils.h"
#include "stats/stats-cluster.h"

#define STATS_CLUSTER_TESTCASE(x) x()

static void
test_stats_cluster_new_replaces_NULL_with_an_empty_string(void)
{
  StatsCluster *sc;
  
  sc = stats_cluster_new(SCS_SOURCE | SCS_FILE, NULL, NULL);
  assert_string(sc->id, "", "StatsCluster->id is not properly defaulted to an empty string");
  assert_string(sc->instance, "", "StatsCluster->instance is not properly defaulted to an empty string");
  stats_cluster_free(sc);
}

static void
assert_stats_cluster_equals(StatsCluster *sc1, StatsCluster *sc2)
{
  assert_true(stats_cluster_equal(sc1, sc2), "unexpected unequal StatsClusters");
}

static void
assert_stats_cluster_mismatches(StatsCluster *sc1, StatsCluster *sc2)
{
  assert_false(stats_cluster_equal(sc1, sc2), "unexpected equal StatsClusters");
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

static void
test_stats_cluster_equal_if_component_id_and_instance_are_the_same(void)
{
  assert_stats_cluster_equals_and_free(stats_cluster_new(SCS_SOURCE | SCS_FILE, "id", "instance"),
                                       stats_cluster_new(SCS_SOURCE | SCS_FILE, "id", "instance"));

  assert_stats_cluster_mismatches_and_free(stats_cluster_new(SCS_SOURCE | SCS_FILE, "id", "instance1"),
                                           stats_cluster_new(SCS_SOURCE | SCS_FILE, "id", "instance2"));

  assert_stats_cluster_mismatches_and_free(stats_cluster_new(SCS_SOURCE | SCS_FILE, "id1", "instance"),
                                           stats_cluster_new(SCS_SOURCE | SCS_FILE, "id2", "instance"));

  assert_stats_cluster_mismatches_and_free(stats_cluster_new(SCS_SOURCE | SCS_FILE, "id", "instance"),
                                           stats_cluster_new(SCS_DESTINATION | SCS_FILE, "id", "instance"));
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
  assert_true(t >= 0, "foreach counter returned a new counter, but we expected the end already");
  assert_gint(type, t, "Counter type mismatch");
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
  
  assert_gint(type_count, st.validate_count, "the number of validated counters mismatch the expected size");
}

static void
test_stats_foreach_counter_yields_tracked_counters(void)
{
  StatsCluster *sc = stats_cluster_new(SCS_SOURCE | SCS_FILE, "id", "instance");

  assert_stats_foreach_yielded_counters_matches(sc, -1);

  stats_cluster_track_counter(sc, SC_TYPE_PROCESSED);
  assert_stats_foreach_yielded_counters_matches(sc, SC_TYPE_PROCESSED, -1);

  stats_cluster_track_counter(sc, SC_TYPE_STAMP);
  assert_stats_foreach_yielded_counters_matches(sc, SC_TYPE_PROCESSED, SC_TYPE_STAMP, -1);
  stats_cluster_free(sc);
}

static void
test_stats_foreach_counter_never_forgets_untracked_counters(void)
{
  StatsCluster *sc = stats_cluster_new(SCS_SOURCE | SCS_FILE, "id", "instance");
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
  StatsCluster *sc = stats_cluster_new(component, NULL, NULL);
  
  name = stats_cluster_get_component_name(sc, buf, sizeof(buf));
  assert_string(name, expected, "component name mismatch");
  stats_cluster_free(sc);
}

static void
test_get_component_name_translates_component_to_name_properly(void)
{
  assert_stats_component_name(SCS_SOURCE | SCS_FILE, "src.file");
  assert_stats_component_name(SCS_DESTINATION | SCS_FILE, "dst.file");
  assert_stats_component_name(SCS_GLOBAL, "global");
  assert_stats_component_name(SCS_SOURCE | SCS_GROUP, "source");
  assert_stats_component_name(SCS_DESTINATION | SCS_GROUP, "destination");
}

static void
test_stats_cluster(void)
{
  STATS_CLUSTER_TESTCASE(test_stats_cluster_new_replaces_NULL_with_an_empty_string);
  STATS_CLUSTER_TESTCASE(test_stats_cluster_equal_if_component_id_and_instance_are_the_same);
  STATS_CLUSTER_TESTCASE(test_stats_foreach_counter_yields_tracked_counters);
  STATS_CLUSTER_TESTCASE(test_stats_foreach_counter_never_forgets_untracked_counters);
  STATS_CLUSTER_TESTCASE(test_get_component_name_translates_component_to_name_properly);
}

int
main(int argc, char *argv[])
{
  test_stats_cluster();
  return 0;
}
