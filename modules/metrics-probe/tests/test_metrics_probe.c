/*
 * Copyright (c) 2023 Attila Szakacs
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include <criterion/criterion.h>

#include "metrics-probe.h"
#include "apphook.h"
#include "stats/stats-cluster-single.h"

static void
_add_label(LogParser *s, const gchar *label, const gchar *value_template_str)
{
  LogTemplate *value_template = log_template_new(s->super.cfg, NULL);
  log_template_compile(value_template, value_template_str, NULL);
  metrics_template_add_label_template(metrics_probe_get_metrics_template(s), label, value_template);
  log_template_unref(value_template);
}

static gboolean
_stats_cluster_exists(const gchar *key, StatsClusterLabel *labels, gsize labels_len)
{
  StatsCluster *cluster = NULL;

  StatsClusterKey sc_key;
  stats_cluster_single_key_set(&sc_key, key, labels, labels_len);

  stats_lock();
  {
    cluster = stats_get_cluster(&sc_key);
  }
  stats_unlock();

  return cluster != NULL;
}

static gsize
_get_stats_counter_value(const gchar *key, StatsClusterLabel *labels, gsize labels_len)
{
  gsize value = 0;
  StatsClusterKey sc_key;
  stats_cluster_single_key_set(&sc_key, key, labels, labels_len);

  stats_lock();
  {
    StatsCounterItem *counter;
    StatsCluster *cluster = stats_register_dynamic_counter(0, &sc_key, SC_TYPE_SINGLE_VALUE, &counter);
    cr_assert(cluster, "Cluster does not exist, probably we have reached max-stats-dynamics().");

    value = stats_counter_get(counter);

    stats_unregister_dynamic_counter(cluster, SC_TYPE_SINGLE_VALUE, &counter);
  }
  stats_unlock();

  return value;
}

static GString *
_format_labels(const StatsClusterLabel *labels, gsize labels_len)
{
  GString *formatted = g_string_new("(");

  for (gint i = 0; i < labels_len; i++)
    {
      if (i != 0)
        g_string_append(formatted, ", ");

      g_string_append(formatted, labels[i].name);
      g_string_append(formatted, " => ");
      g_string_append(formatted, labels[i].value);
    }

  g_string_append_c(formatted, ')');

  return formatted;
}

static void
_assert_counter_value(const gchar *key, StatsClusterLabel *labels, gsize labels_len, gsize expected_value)
{
  gsize actual_value = _get_stats_counter_value(key, labels, labels_len);
  GString *formatted_labels = (expected_value == actual_value) ? NULL : _format_labels(labels, labels_len);

  cr_assert(expected_value == actual_value,
            "Unexpected counter value. key: %s labels: %s expected: %lu actual: %lu",
            key, formatted_labels->str, expected_value, actual_value);

  if (formatted_labels)
    g_string_free(formatted_labels, TRUE);
}

Test(metrics_probe, test_metrics_probe_defaults)
{
  LogParser *tmp_metrics_probe = metrics_probe_new(configuration);
  LogParser *metrics_probe = (LogParser *) log_pipe_clone(&tmp_metrics_probe->super);
  log_pipe_unref(&tmp_metrics_probe->super);
  cr_assert(log_pipe_init(&metrics_probe->super), "Failed to init metrics-probe");

  LogMessage *msg = log_msg_new_empty();
  log_msg_set_value_by_name(msg, "APP", "app_1", -1);
  log_msg_set_value_by_name(msg, "HOST", "host_1", -1);
  log_msg_set_value_by_name(msg, "PROGRAM", "program_1", -1);
  log_msg_set_value_by_name(msg, "SOURCE", "source_1", -1);

  StatsClusterLabel expected_labels_1[] =
  {
    stats_cluster_label("app", "app_1"),
    stats_cluster_label("host", "host_1"),
    stats_cluster_label("program", "program_1"),
    stats_cluster_label("source", "source_1"),
  };

  cr_assert(log_parser_process(metrics_probe, &msg, NULL, "", -1), "Failed to apply metrics-probe");
  _assert_counter_value("classified_events_total",
                        expected_labels_1,
                        G_N_ELEMENTS(expected_labels_1),
                        1);

  cr_assert(log_parser_process(metrics_probe, &msg, NULL, "", -1), "Failed to apply metrics-probe");
  _assert_counter_value("classified_events_total",
                        expected_labels_1,
                        G_N_ELEMENTS(expected_labels_1),
                        2);

  log_msg_unref(msg);
  msg = log_msg_new_empty();
  log_msg_set_value_by_name(msg, "APP", "app_2", -1);
  log_msg_set_value_by_name(msg, "HOST", "host_2", -1);
  log_msg_set_value_by_name(msg, "PROGRAM", "program_2", -1);
  log_msg_set_value_by_name(msg, "SOURCE", "source_2", -1);

  StatsClusterLabel expected_labels_2[] =
  {
    stats_cluster_label("app", "app_2"),
    stats_cluster_label("host", "host_2"),
    stats_cluster_label("program", "program_2"),
    stats_cluster_label("source", "source_2"),
  };

  cr_assert(log_parser_process(metrics_probe, &msg, NULL, "", -1), "Failed to apply metrics-probe");
  _assert_counter_value("classified_events_total",
                        expected_labels_2,
                        G_N_ELEMENTS(expected_labels_2),
                        1);

  log_msg_unref(msg);
  log_pipe_deinit(&metrics_probe->super);
  log_pipe_unref(&metrics_probe->super);
}

Test(metrics_probe, test_metrics_probe_custom_labels_only)
{
  LogParser *tmp_metrics_probe = metrics_probe_new(configuration);
  _add_label(tmp_metrics_probe, "test_label", "foo");

  LogParser *metrics_probe = (LogParser *) log_pipe_clone(&tmp_metrics_probe->super);
  log_pipe_unref(&tmp_metrics_probe->super);

  cr_assert_not(log_pipe_init(&metrics_probe->super), "metrics-probe should have failed to init");

  log_pipe_unref(&metrics_probe->super);
}

Test(metrics_probe, test_metrics_probe_custom_key_only)
{
  LogParser *tmp_metrics_probe = metrics_probe_new(configuration);
  metrics_template_set_key(metrics_probe_get_metrics_template(tmp_metrics_probe), "custom_key");

  LogParser *metrics_probe = (LogParser *) log_pipe_clone(&tmp_metrics_probe->super);
  log_pipe_unref(&tmp_metrics_probe->super);
  cr_assert(log_pipe_init(&metrics_probe->super), "Failed to init metrics-probe");

  LogMessage *msg = log_msg_new_empty();
  StatsClusterLabel expected_labels[] = {};

  cr_assert(log_parser_process(metrics_probe, &msg, NULL, "", -1), "Failed to apply metrics-probe");
  _assert_counter_value("custom_key",
                        expected_labels,
                        G_N_ELEMENTS(expected_labels),
                        1);

  cr_assert(log_parser_process(metrics_probe, &msg, NULL, "", -1), "Failed to apply metrics-probe");
  _assert_counter_value("custom_key",
                        expected_labels,
                        G_N_ELEMENTS(expected_labels),
                        2);

  log_msg_unref(msg);
  log_pipe_deinit(&metrics_probe->super);
  log_pipe_unref(&metrics_probe->super);
}

Test(metrics_probe, test_metrics_probe_custom_full)
{
  LogParser *tmp_metrics_probe = metrics_probe_new(configuration);
  metrics_template_set_key(metrics_probe_get_metrics_template(tmp_metrics_probe), "custom_key");

  _add_label(tmp_metrics_probe, "test_label_3", "foo");
  _add_label(tmp_metrics_probe, "test_label_1", "${test_field_1}");
  _add_label(tmp_metrics_probe, "test_label_2", "${test_field_2}");

  LogParser *metrics_probe = (LogParser *) log_pipe_clone(&tmp_metrics_probe->super);
  log_pipe_unref(&tmp_metrics_probe->super);
  cr_assert(log_pipe_init(&metrics_probe->super), "Failed to init metrics-probe");

  LogMessage *msg = log_msg_new_empty();
  log_msg_set_value_by_name(msg, "test_field_1", "test_value_1", -1);
  log_msg_set_value_by_name(msg, "test_field_2", "test_value_2", -1);

  StatsClusterLabel expected_labels_1[] =
  {
    stats_cluster_label("test_label_1", "test_value_1"),
    stats_cluster_label("test_label_2", "test_value_2"),
    stats_cluster_label("test_label_3", "foo"),
  };

  cr_assert(log_parser_process(metrics_probe, &msg, NULL, "", -1), "Failed to apply metrics-probe");
  _assert_counter_value("custom_key",
                        expected_labels_1,
                        G_N_ELEMENTS(expected_labels_1),
                        1);

  cr_assert(log_parser_process(metrics_probe, &msg, NULL, "", -1), "Failed to apply metrics-probe");
  _assert_counter_value("custom_key",
                        expected_labels_1,
                        G_N_ELEMENTS(expected_labels_1),
                        2);

  log_msg_unref(msg);
  msg = log_msg_new_empty();
  log_msg_set_value_by_name(msg, "test_field_1", "test_value_3", -1);
  log_msg_set_value_by_name(msg, "test_field_2", "test_value_4", -1);

  StatsClusterLabel expected_labels_2[] =
  {
    stats_cluster_label("test_label_1", "test_value_3"),
    stats_cluster_label("test_label_2", "test_value_4"),
    stats_cluster_label("test_label_3", "foo"),
  };

  cr_assert(log_parser_process(metrics_probe, &msg, NULL, "", -1), "Failed to apply metrics-probe");
  _assert_counter_value("custom_key",
                        expected_labels_2,
                        G_N_ELEMENTS(expected_labels_2),
                        1);

  log_msg_unref(msg);
  log_pipe_deinit(&metrics_probe->super);
  log_pipe_unref(&metrics_probe->super);
}

Test(metrics_probe, test_metrics_probe_stats_max_dynamics)
{
  configuration->stats_options.max_dynamic = 1;
  stats_reinit(&configuration->stats_options);

  LogParser *tmp_metrics_probe = metrics_probe_new(configuration);
  metrics_template_set_key(metrics_probe_get_metrics_template(tmp_metrics_probe), "custom_key");
  _add_label(tmp_metrics_probe, "test_label", "${test_field}");

  LogParser *metrics_probe = (LogParser *) log_pipe_clone(&tmp_metrics_probe->super);
  log_pipe_unref(&tmp_metrics_probe->super);
  cr_assert(log_pipe_init(&metrics_probe->super), "Failed to init metrics-probe");

  LogMessage *msg = log_msg_new_empty();
  log_msg_set_value_by_name(msg, "test_field", "test_value_1", -1);

  StatsClusterLabel expected_labels_1[] =
  {
    stats_cluster_label("test_label", "test_value_1"),
  };

  cr_assert(log_parser_process(metrics_probe, &msg, NULL, "", -1), "Failed to apply metrics-probe");
  _assert_counter_value("custom_key",
                        expected_labels_1,
                        G_N_ELEMENTS(expected_labels_1),
                        1);

  log_msg_unref(msg);
  msg = log_msg_new_empty();
  log_msg_set_value_by_name(msg, "test_field", "test_value_2", -1);

  StatsClusterLabel expected_labels_2[] =
  {
    stats_cluster_label("test_label", "test_value_2"),
  };

  cr_assert(log_parser_process(metrics_probe, &msg, NULL, "", -1), "Failed to apply metrics-probe");
  cr_assert_not(_stats_cluster_exists("custom_key", expected_labels_2, G_N_ELEMENTS(expected_labels_2)));

  log_msg_unref(msg);
  log_pipe_deinit(&metrics_probe->super);
  log_pipe_unref(&metrics_probe->super);
}

Test(metrics_probe, test_metrics_probe_increment)
{
  LogParser *tmp_metrics_probe = metrics_probe_new(configuration);
  metrics_template_set_key(metrics_probe_get_metrics_template(tmp_metrics_probe), "custom_key");
  LogTemplate *increment_template = log_template_new(tmp_metrics_probe->super.cfg, NULL);
  log_template_compile(increment_template, "${custom_increment}", NULL);
  metrics_probe_set_increment_template(tmp_metrics_probe, increment_template);
  log_template_unref(increment_template);

  LogParser *metrics_probe = (LogParser *) log_pipe_clone(&tmp_metrics_probe->super);
  log_pipe_unref(&tmp_metrics_probe->super);
  cr_assert(log_pipe_init(&metrics_probe->super), "Failed to init metrics-probe");

  LogMessage *msg = log_msg_new_empty();
  log_msg_set_value_by_name(msg, "custom_increment", "1337", -1);
  StatsClusterLabel expected_labels[] = {};

  cr_assert(log_parser_process(metrics_probe, &msg, NULL, "", -1), "Failed to apply metrics-probe");
  _assert_counter_value("custom_key",
                        expected_labels,
                        G_N_ELEMENTS(expected_labels),
                        1337);

  log_msg_unref(msg);
  log_pipe_deinit(&metrics_probe->super);
  log_pipe_unref(&metrics_probe->super);
}

Test(metrics_probe, test_metrics_probe_level)
{
  LogParser *tmp_metrics_probe = metrics_probe_new(configuration);
  metrics_template_set_key(metrics_probe_get_metrics_template(tmp_metrics_probe), "custom_key");
  metrics_template_set_level(metrics_probe_get_metrics_template(tmp_metrics_probe), STATS_LEVEL2);

  LogParser *metrics_probe = (LogParser *) log_pipe_clone(&tmp_metrics_probe->super);
  log_pipe_unref(&tmp_metrics_probe->super);
  cr_assert(log_pipe_init(&metrics_probe->super), "Failed to init metrics-probe");

  LogMessage *msg = log_msg_new_empty();
  StatsClusterLabel expected_labels[] = {};

  configuration->stats_options.level = STATS_LEVEL0;
  cr_assert(cfg_init(configuration));
  cr_assert(log_parser_process(metrics_probe, &msg, NULL, "", -1), "Failed to apply metrics-probe");
  cr_assert_not(_stats_cluster_exists("custom_key", expected_labels, G_N_ELEMENTS(expected_labels)));
  cr_assert(cfg_deinit(configuration));

  configuration->stats_options.level = STATS_LEVEL1;
  cr_assert(cfg_init(configuration));
  cr_assert(log_parser_process(metrics_probe, &msg, NULL, "", -1), "Failed to apply metrics-probe");
  cr_assert_not(_stats_cluster_exists("custom_key", expected_labels, G_N_ELEMENTS(expected_labels)));
  cr_assert(cfg_deinit(configuration));

  configuration->stats_options.level = STATS_LEVEL2;
  cr_assert(cfg_init(configuration));
  cr_assert(log_parser_process(metrics_probe, &msg, NULL, "", -1), "Failed to apply metrics-probe");
  _assert_counter_value("custom_key",
                        expected_labels,
                        G_N_ELEMENTS(expected_labels),
                        1);
  cr_assert(cfg_deinit(configuration));

  log_msg_unref(msg);
  log_pipe_deinit(&metrics_probe->super);
  log_pipe_unref(&metrics_probe->super);
}

Test(metrics_probe, test_metrics_probe_dynamic_labels)
{
  LogParser *tmp_metrics_probe = metrics_probe_new(configuration);
  metrics_template_set_key(metrics_probe_get_metrics_template(tmp_metrics_probe), "custom_key");
  _add_label(tmp_metrics_probe, "test_label", "${test_field}");
  ValuePairs *vp = metrics_template_get_value_pairs(metrics_probe_get_metrics_template(tmp_metrics_probe));
  value_pairs_add_glob_pattern(vp, "test_prefix.*", TRUE);

  LogParser *metrics_probe = (LogParser *) log_pipe_clone(&tmp_metrics_probe->super);
  log_pipe_unref(&tmp_metrics_probe->super);
  cr_assert(log_pipe_init(&metrics_probe->super), "Failed to init metrics-probe");

  LogMessage *msg = log_msg_new_empty();
  log_msg_set_value_by_name(msg, "test_field", "test_field_value", -1);
  log_msg_set_value_by_name(msg, "test_prefix.test_field_1", "test_prefix_test_field_1_value", -1);
  log_msg_set_value_by_name(msg, "test_prefix.test_field_2", "test_prefix_test_field_2_value", -1);

  StatsClusterLabel expected_labels[] =
  {
    stats_cluster_label("test_label", "test_field_value"),
    stats_cluster_label("test_prefix.test_field_1", "test_prefix_test_field_1_value"),
    stats_cluster_label("test_prefix.test_field_2", "test_prefix_test_field_2_value"),
  };

  cr_assert(log_parser_process(metrics_probe, &msg, NULL, "", -1), "Failed to apply metrics-probe");
  _assert_counter_value("custom_key",
                        expected_labels,
                        G_N_ELEMENTS(expected_labels),
                        1);

  cr_assert(log_parser_process(metrics_probe, &msg, NULL, "", -1), "Failed to apply metrics-probe");
  _assert_counter_value("custom_key",
                        expected_labels,
                        G_N_ELEMENTS(expected_labels),
                        2);

  log_msg_unref(msg);
  log_pipe_deinit(&metrics_probe->super);
  log_pipe_unref(&metrics_probe->super);
}

void setup(void)
{
  app_startup();
  configuration = cfg_new_snippet();
}

void teardown(void)
{
  app_shutdown();
}

TestSuite(metrics_probe, .init = setup, .fini = teardown);
