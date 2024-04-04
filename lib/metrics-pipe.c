/*
 * Copyright (c) 2023 Attila Szakacs
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

#include "metrics-pipe.h"
#include "stats/stats-registry.h"
#include "stats/stats-cluster-single.h"

static void
_init_stats_keys(MetricsPipe *self, StatsClusterKey *ingress_sc_key, StatsClusterKey *egress_sc_key)
{
  enum { labels_len = 1 };
  static StatsClusterLabel labels[labels_len];

  labels[0] = stats_cluster_label("id", self->log_path_name);
  stats_cluster_single_key_set(ingress_sc_key, "route_ingress_total", labels, labels_len);
  stats_cluster_single_key_set(egress_sc_key, "route_egress_total", labels, labels_len);
}

static void
_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  MetricsPipe *self = (MetricsPipe *) s;

  stats_counter_inc(self->ingress_counter);

  gboolean matched = TRUE;
  LogPathOptions local_path_options;
  log_path_options_chain(&local_path_options, path_options);
  local_path_options.matched = &matched;

  log_pipe_forward_msg(s, msg, &local_path_options);

  /* this is populated via local_path_options->matched */
  if (matched)
    stats_counter_inc(self->egress_counter);

  if (path_options->matched && !matched)
    *path_options->matched = FALSE;
}

static gboolean
_init(LogPipe *s)
{
  MetricsPipe *self = (MetricsPipe *) s;

  StatsClusterKey ingress_sc_key;
  StatsClusterKey egress_sc_key;
  _init_stats_keys(self, &ingress_sc_key, &egress_sc_key);

  stats_lock();
  {
    stats_register_counter(STATS_LEVEL1, &ingress_sc_key, SC_TYPE_SINGLE_VALUE, &self->ingress_counter);
    stats_register_counter(STATS_LEVEL1, &egress_sc_key, SC_TYPE_SINGLE_VALUE, &self->egress_counter);
  }
  stats_unlock();

  return TRUE;
}

static gboolean
_deinit(LogPipe *s)
{
  MetricsPipe *self = (MetricsPipe *) s;

  StatsClusterKey ingress_sc_key;
  StatsClusterKey egress_sc_key;
  _init_stats_keys(self, &ingress_sc_key, &egress_sc_key);

  stats_lock();
  {
    stats_unregister_counter(&ingress_sc_key, SC_TYPE_SINGLE_VALUE, &self->ingress_counter);
    stats_unregister_counter(&egress_sc_key, SC_TYPE_SINGLE_VALUE, &self->egress_counter);
  }
  stats_unlock();

  return TRUE;
}

static void
_free(LogPipe *s)
{
  MetricsPipe *self = (MetricsPipe *) s;

  g_free(self->log_path_name);
  log_pipe_free_method(s);
}

MetricsPipe *
metrics_pipe_new(GlobalConfig *cfg, const gchar *log_path_name)
{
  MetricsPipe *self = g_new0(MetricsPipe, 1);

  log_pipe_init_instance(&self->super, cfg);
  self->super.queue = _queue;
  self->super.init = _init;
  self->super.deinit = _deinit;
  self->super.free_fn = _free;

  self->log_path_name = g_strdup(log_path_name);

  log_pipe_add_info(&self->super, self->log_path_name);

  return self;
}
