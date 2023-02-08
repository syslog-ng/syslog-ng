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

static LogParser *
_create_metrics_probe(void)
{
  LogParser *temp_metrics_probe = metrics_probe_new(configuration);
  LogParser *metrics_probe = (LogParser *) log_pipe_clone(&temp_metrics_probe->super);
  log_pipe_unref(&temp_metrics_probe->super);

  return metrics_probe;
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
  LogParser *metrics_probe = _create_metrics_probe();
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
