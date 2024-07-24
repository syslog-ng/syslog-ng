/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2023-2024 László Várady
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

#include "stats/stats.h"
#include "stats/stats-cluster.h"
#include "stats/stats-cluster-single.h"
#include "stats/stats-cluster-logpipe.h"
#include "stats/stats-prometheus.h"
#include "timeutils/unixtime.h"
#include "scratch-buffers.h"
#include "mainloop.h"
#include "apphook.h"
#include "libtest/fake-time.h"

#include <float.h>
#include <limits.h>

static void
setup(void)
{
  app_startup();
}

static void
teardown(void)
{
  scratch_buffers_explicit_gc();
  app_shutdown();
}

TestSuite(stats_prometheus, .init = setup, .fini = teardown);

static inline StatsCluster *
test_single_cluster(const gchar *name, StatsClusterLabel *labels, gsize labels_len)
{
  StatsClusterKey key;
  stats_cluster_single_key_set(&key, name, labels, labels_len);
  return stats_cluster_new(&key);
}

static inline StatsCluster *
test_logpipe_cluster(const gchar *name, StatsClusterLabel *labels, gsize labels_len)
{
  StatsClusterKey key;
  stats_cluster_logpipe_key_set(&key, name, labels, labels_len);
  return stats_cluster_new(&key);
}

static inline void
assert_prometheus_format(StatsCluster *cluster, gint type,  const gchar *expected_prom_record)
{
  cr_assert_str_eq(stats_prometheus_format_counter(cluster, type, stats_cluster_get_counter(cluster, type))->str,
                   expected_prom_record);
}

Test(stats_prometheus, test_prometheus_format_single)
{
  StatsCluster *cluster = test_single_cluster("test_name", NULL, 0);
  assert_prometheus_format(cluster, SC_TYPE_SINGLE_VALUE, "syslogng_test_name 0\n");
  stats_cluster_free(cluster);

  StatsClusterLabel labels[] = { stats_cluster_label("app", "cisco") };
  cluster = test_single_cluster("test_name", labels, G_N_ELEMENTS(labels));
  assert_prometheus_format(cluster, SC_TYPE_SINGLE_VALUE, "syslogng_test_name{app=\"cisco\"} 0\n");
  stats_cluster_free(cluster);

  StatsClusterLabel labels2[] =
  {
    stats_cluster_label("app", "cisco"),
    stats_cluster_label("sourceip", "127.0.0.1"),
    stats_cluster_label("customlabel", "value"),
  };
  cluster = test_single_cluster("test_name", labels2, G_N_ELEMENTS(labels2));
  stats_counter_inc(stats_cluster_track_counter(cluster, SC_TYPE_SINGLE_VALUE));
  assert_prometheus_format(cluster, SC_TYPE_SINGLE_VALUE,
                           "syslogng_test_name{app=\"cisco\",sourceip=\"127.0.0.1\",customlabel=\"value\"} 1\n");
  stats_cluster_free(cluster);
}

Test(stats_prometheus, test_prometheus_format_logpipe)
{
  StatsCluster *cluster = test_logpipe_cluster("test_name", NULL, 0);
  assert_prometheus_format(cluster, SC_TYPE_PROCESSED, "syslogng_test_name{result=\"processed\"} 0\n");
  stats_cluster_free(cluster);

  StatsClusterLabel labels[] = { stats_cluster_label("app", "cisco") };
  cluster = test_logpipe_cluster("test_name", labels, G_N_ELEMENTS(labels));
  assert_prometheus_format(cluster, SC_TYPE_DROPPED, "syslogng_test_name{app=\"cisco\",result=\"dropped\"} 0\n");
  stats_cluster_free(cluster);

  StatsClusterLabel labels2[] =
  {
    stats_cluster_label("app", "cisco"),
    stats_cluster_label("sourceip", "127.0.0.1"),
    stats_cluster_label("customlabel", "value"),
  };
  cluster = test_logpipe_cluster("test_name", labels2, G_N_ELEMENTS(labels2));
  stats_counter_inc(stats_cluster_track_counter(cluster, SC_TYPE_WRITTEN));
  assert_prometheus_format(cluster, SC_TYPE_WRITTEN,
                           "syslogng_test_name{app=\"cisco\",sourceip=\"127.0.0.1\",customlabel=\"value\","
                           "result=\"delivered\"} 1\n");
  stats_cluster_free(cluster);
}

Test(stats_prometheus, test_prometheus_format_empty_label_value)
{
  StatsClusterLabel labels[] =
  {
    stats_cluster_label("app", ""),
    stats_cluster_label("sourceip", NULL),
    stats_cluster_label("customlabel", "value"),
  };
  StatsCluster *cluster = test_single_cluster("test_name", labels, G_N_ELEMENTS(labels));
  assert_prometheus_format(cluster, SC_TYPE_SINGLE_VALUE, "syslogng_test_name{customlabel=\"value\"} 0\n");
  stats_cluster_free(cluster);

  StatsClusterLabel labels2[] =
  {
    stats_cluster_label("app", NULL),
    stats_cluster_label("sourceip", ""),
  };
  cluster = test_single_cluster("test_name", labels2, G_N_ELEMENTS(labels2));
  assert_prometheus_format(cluster, SC_TYPE_SINGLE_VALUE, "syslogng_test_name 0\n");
  stats_cluster_free(cluster);

  cluster = test_logpipe_cluster("test_name", labels2, G_N_ELEMENTS(labels2));
  assert_prometheus_format(cluster, SC_TYPE_PROCESSED, "syslogng_test_name{result=\"processed\"} 0\n");
  stats_cluster_free(cluster);
}

Test(stats_prometheus, test_prometheus_format_sanitize)
{
  StatsClusterLabel labels[] =
  {
    stats_cluster_label("app.name:", "a"),
    stats_cluster_label("//source-ip\n", "\"b\""),
    stats_cluster_label("laűúőbel", "c\n"),
  };
  StatsCluster *cluster = test_single_cluster("test.name-http://localhost/ű#", labels, G_N_ELEMENTS(labels));
  assert_prometheus_format(cluster, SC_TYPE_SINGLE_VALUE,
                           "syslogng_test_name_httplocalhost{app_name=\"a\",source_ip=\"\\\"b\\\"\",label=\"c\\n\"} 0\n");
  stats_cluster_free(cluster);
}

Test(stats_prometheus, test_prometheus_format_label_escaping)
{
  /* Exposition format:
   *
   * A label value can be any sequence of UTF-8 characters, but the
   * backslash (\), double-quote ("), and line feed (\n) characters have to be
   * escaped as \\, \", and \n, respectively.
   */

  StatsClusterLabel labels[] =
  {
    stats_cluster_label("control_chars", "a\a\tb\nc"),
    stats_cluster_label("backslashes", "a\\a\\t\\nb"),
    stats_cluster_label("quotes", "\"\"hello\""),
    stats_cluster_label("invalid_utf8", "a\xfa"),
  };
  StatsCluster *cluster = test_single_cluster("test_name", labels, G_N_ELEMENTS(labels));

  assert_prometheus_format(cluster, SC_TYPE_SINGLE_VALUE,
                           "syslogng_test_name{control_chars=\"a\a\tb\\nc\","
                           "backslashes=\"a\\\\a\\\\t\\\\nb\","
                           "quotes=\"\\\"\\\"hello\\\"\","
                           "invalid_utf8=\"a\\\\xfa\"} 0\n");

  stats_cluster_free(cluster);
}

gchar *stats_format_prometheus_format_value(const StatsClusterKey *key, const StatsCounterItem *counter);

Test(stats_prometheus, test_prometheus_format_value)
{
  StatsCounterItem counter = {0};
  stats_counter_set(&counter, 9);

  StatsClusterKey key;
  stats_cluster_single_key_set(&key, "name", NULL, 0);

  stats_cluster_single_key_add_unit(&key, SCU_NONE);
  cr_assert_str_eq(stats_format_prometheus_format_value(&key, &counter), "9");

  stats_cluster_single_key_add_unit(&key, SCU_GIB);
  cr_assert_str_eq(stats_format_prometheus_format_value(&key, &counter), "9663676416");
  stats_cluster_single_key_add_unit(&key, SCU_MIB);
  cr_assert_str_eq(stats_format_prometheus_format_value(&key, &counter), "9437184");
  stats_cluster_single_key_add_unit(&key, SCU_KIB);
  cr_assert_str_eq(stats_format_prometheus_format_value(&key, &counter), "9216");
  stats_cluster_single_key_add_unit(&key, SCU_BYTES);
  cr_assert_str_eq(stats_format_prometheus_format_value(&key, &counter), "9");

  stats_cluster_single_key_add_unit(&key, SCU_HOURS);
  cr_assert_str_eq(stats_format_prometheus_format_value(&key, &counter), "32400");
  stats_cluster_single_key_add_unit(&key, SCU_MINUTES);
  cr_assert_str_eq(stats_format_prometheus_format_value(&key, &counter), "540");
  stats_cluster_single_key_add_unit(&key, SCU_SECONDS);
  cr_assert_str_eq(stats_format_prometheus_format_value(&key, &counter), "9");

  stats_cluster_single_key_add_unit(&key, SCU_MILLISECONDS);
  gdouble actual = g_ascii_strtod(stats_format_prometheus_format_value(&key, &counter), NULL);
  cr_assert_float_eq(actual, 0.009L, DBL_EPSILON);

  stats_cluster_single_key_add_unit(&key, SCU_NANOSECONDS);
  actual = g_ascii_strtod(stats_format_prometheus_format_value(&key, &counter), NULL);
  cr_assert_float_eq(actual, 9e-9, DBL_EPSILON);

  /* Relative to time of query */
  stats_cluster_single_key_add_frame_of_reference(&key, SCFOR_RELATIVE_TO_TIME_OF_QUERY);

  /* Fri Jan 01 2100 01:01:01 GMT+0000 */
  fake_time(INT_MAX);
  fake_time_add(1954964814);

  /* None, bytes and milli/nanoseconds units are unaffected */
  stats_cluster_single_key_add_unit(&key, SCU_NONE);
  cr_assert_str_eq(stats_format_prometheus_format_value(&key, &counter), "9");
  stats_cluster_single_key_add_unit(&key, SCU_GIB);
  cr_assert_str_eq(stats_format_prometheus_format_value(&key, &counter), "9663676416");
  stats_cluster_single_key_add_unit(&key, SCU_MIB);
  cr_assert_str_eq(stats_format_prometheus_format_value(&key, &counter), "9437184");
  stats_cluster_single_key_add_unit(&key, SCU_KIB);
  cr_assert_str_eq(stats_format_prometheus_format_value(&key, &counter), "9216");
  stats_cluster_single_key_add_unit(&key, SCU_BYTES);
  cr_assert_str_eq(stats_format_prometheus_format_value(&key, &counter), "9");

  stats_cluster_single_key_add_unit(&key, SCU_MILLISECONDS);
  actual = g_ascii_strtod(stats_format_prometheus_format_value(&key, &counter), NULL);
  cr_assert_float_eq(actual, 0.009L, DBL_EPSILON);

  stats_cluster_single_key_add_unit(&key, SCU_NANOSECONDS);
  actual = g_ascii_strtod(stats_format_prometheus_format_value(&key, &counter), NULL);
  cr_assert_float_eq(actual, 9e-9, DBL_EPSILON);

  /* Hours, minutes and seconds are affected */
  stats_cluster_single_key_add_unit(&key, SCU_HOURS);
  cr_assert_str_eq(stats_format_prometheus_format_value(&key, &counter), "4102416061");
  stats_cluster_single_key_add_unit(&key, SCU_MINUTES);
  cr_assert_str_eq(stats_format_prometheus_format_value(&key, &counter), "4102447921");
  stats_cluster_single_key_add_unit(&key, SCU_SECONDS);
  cr_assert_str_eq(stats_format_prometheus_format_value(&key, &counter), "4102448452");
}


Test(stats_prometheus, test_prometheus_format_legacy)
{
  StatsClusterKey key;
  stats_cluster_single_key_legacy_set_with_name(&key, SCS_GLOBAL, "id", NULL, "value");
  StatsCluster *cluster = stats_cluster_new(&key);
  assert_prometheus_format(cluster, SC_TYPE_SINGLE_VALUE, "syslogng_global_id 0\n");
  stats_cluster_free(cluster);

  stats_cluster_single_key_legacy_set_with_name(&key, SCS_GLOBAL, "id", NULL, "custom");
  cluster = stats_cluster_new(&key);
  assert_prometheus_format(cluster, SC_TYPE_SINGLE_VALUE, "syslogng_global_id_custom 0\n");
  stats_cluster_free(cluster);

  stats_cluster_single_key_legacy_set_with_name(&key, SCS_SOURCE | SCS_TAG, "", "instance", "custom");
  cluster = stats_cluster_new(&key);
  assert_prometheus_format(cluster, SC_TYPE_SINGLE_VALUE,
                           "syslogng_src_tag_custom{stat_instance=\"instance\"} 0\n");
  stats_cluster_free(cluster);

  stats_cluster_single_key_legacy_set_with_name(&key, SCS_SOURCE | SCS_TAG, "id", "instance", "custom");
  cluster = stats_cluster_new(&key);
  assert_prometheus_format(cluster, SC_TYPE_SINGLE_VALUE,
                           "syslogng_src_tag_custom{id=\"id\",stat_instance=\"instance\"} 0\n");
  stats_cluster_free(cluster);

  stats_cluster_logpipe_key_legacy_set(&key, SCS_SOURCE | SCS_TAG, "id", "instance");
  cluster = stats_cluster_new(&key);
  assert_prometheus_format(cluster, SC_TYPE_PROCESSED,
                           "syslogng_src_tag_processed{id=\"id\",stat_instance=\"instance\"} 0\n");
  stats_cluster_free(cluster);
}

Test(stats_prometheus, test_prometheus_format_legacy_alias_is_ignored)
{
  StatsClusterKey key;
  stats_cluster_single_key_set(&key, "name", NULL, 0);
  stats_cluster_single_key_add_legacy_alias_with_name(&key, SCS_SOURCE | SCS_TAG, "", "instance", "custom");
  StatsCluster *cluster = stats_cluster_new(&key);
  assert_prometheus_format(cluster, SC_TYPE_SINGLE_VALUE, "syslogng_name 0\n");
  stats_cluster_free(cluster);
}
