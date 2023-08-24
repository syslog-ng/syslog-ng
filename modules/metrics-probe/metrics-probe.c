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

#include "metrics-probe.h"
#include "label-template.h"
#include "stats/stats-cluster-single.h"
#include "scratch-buffers.h"
#include "apphook.h"
#include "tls-support.h"

typedef struct _MetricsProbe
{
  LogParser super;

  gchar *key;
  GList *label_templates;
  GArray *label_buffers;
  guint num_of_static_labels;
  LogTemplate *increment_template;
  gint level;

  LogTemplateOptions template_options;
} MetricsProbe;

TLS_BLOCK_START
{
  GHashTable *clusters;
}
TLS_BLOCK_END;

#define clusters __tls_deref(clusters)

static StatsCluster *
_register_single_cluster_locked(StatsClusterKey *key, gint stats_level)
{
  StatsCluster *cluster;

  stats_lock();
  {
    StatsCounterItem *counter;
    cluster = stats_register_dynamic_counter(stats_level, key, SC_TYPE_SINGLE_VALUE, &counter);
  }
  stats_unlock();

  return cluster;
}

static void
_unregister_single_cluster_locked(StatsCluster *cluster)
{
  stats_lock();
  {
    StatsCounterItem *counter = stats_cluster_single_get_counter(cluster);
    stats_unregister_dynamic_counter(cluster, SC_TYPE_SINGLE_VALUE, &counter);
  }
  stats_unlock();
}

void
metrics_probe_set_key(LogParser *s, const gchar *key)
{
  MetricsProbe *self = (MetricsProbe *) s;

  g_free(self->key);
  self->key = g_strdup(key);
}

void
metrics_probe_add_label_template(LogParser *s, const gchar *label, LogTemplate *value_template)
{
  MetricsProbe *self = (MetricsProbe *) s;

  self->label_templates = g_list_append(self->label_templates, label_template_new(label, value_template));
}

void
metrics_probe_set_increment_template(LogParser *s, LogTemplate *increment_template)
{
  MetricsProbe *self = (MetricsProbe *) s;

  log_template_unref(self->increment_template);
  self->increment_template = log_template_ref(increment_template);
}

void
metrics_probe_set_level(LogParser *s, gint level)
{
  MetricsProbe *self = (MetricsProbe *) s;

  self->level = level;
}

LogTemplateOptions *
metrics_probe_get_template_options(LogParser *s)
{
  MetricsProbe *self = (MetricsProbe *) s;

  return &self->template_options;
}


static void
_calculate_stats_cluster_key(MetricsProbe *self, LogMessage *msg, StatsClusterKey *key)
{
  gint label_idx = 0;
  for (GList *elem = g_list_first(self->label_templates); elem; elem = elem->next)
    {
      LabelTemplate *label_template = (LabelTemplate *) elem->data;
      GString *value_buffer = scratch_buffers_alloc();

      label_template_format(label_template, &self->template_options, msg, value_buffer,
                            &g_array_index(self->label_buffers, StatsClusterLabel, label_idx));
      label_idx++;
    }

  stats_cluster_single_key_set(key, self->key, (StatsClusterLabel *) self->label_buffers->data,
                               self->label_buffers->len);
}

static StatsCounterItem *
_lookup_stats_counter(MetricsProbe *self, LogMessage *msg)
{
  StatsClusterKey key;
  ScratchBuffersMarker marker;

  scratch_buffers_mark(&marker);
  _calculate_stats_cluster_key(self, msg, &key);

  StatsCluster *cluster = g_hash_table_lookup(clusters, &key);
  if (!cluster)
    {
      cluster = _register_single_cluster_locked(&key, self->level);
      if (cluster)
        g_hash_table_insert(clusters, &cluster->key, cluster);
    }

  scratch_buffers_reclaim_marked(marker);

  return stats_cluster_single_get_counter(cluster);
}

static const gchar *
_format_increment_template(MetricsProbe *self, LogMessage *msg, GString *buffer)
{
  if (log_template_is_trivial(self->increment_template))
    {
      gssize len;
      return log_template_get_trivial_value(self->increment_template, msg, &len);
    }

  LogTemplateEvalOptions template_eval_options = { &self->template_options, LTZ_SEND, 0, NULL, LM_VT_STRING };
  log_template_format(self->increment_template, msg, &template_eval_options, buffer);

  return buffer->str;
}

static gssize
_calculate_increment(MetricsProbe *self, LogMessage *msg)
{
  if (!self->increment_template)
    return 1;

  ScratchBuffersMarker marker;
  GString *increment_buffer = scratch_buffers_alloc_and_mark(&marker);

  const gchar *increment_str = _format_increment_template(self, msg, increment_buffer);
  gssize increment = strtoll(increment_str, NULL, 10);

  scratch_buffers_reclaim_marked(marker);

  return increment;
}

static gboolean
_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input, gsize input_len)
{
  MetricsProbe *self = (MetricsProbe *) s;

  msg_trace("metrics-probe message processing started",
            evt_tag_str("key", self->key),
            evt_tag_msg_reference(*pmsg));

  if (!stats_check_level(self->level))
    return TRUE;

  StatsCounterItem *counter = _lookup_stats_counter(self, *pmsg);
  gssize increment = _calculate_increment(self, *pmsg);
  stats_counter_add(counter, increment);

  return TRUE;
}

static void
_add_default_label_template(MetricsProbe *self, const gchar *label, const gchar *value_template_str)
{
  LogTemplate *value_template = log_template_new(self->super.super.cfg, NULL);
  log_template_compile(value_template, value_template_str, NULL);
  metrics_probe_add_label_template(&self->super, label, value_template);
  log_template_unref(value_template);
}

static void
_init_tls_clusters_map_thread_init_hook(gpointer user_data)
{
  g_assert(!clusters);

  clusters = g_hash_table_new_full((GHashFunc) stats_cluster_key_hash,
                                   (GEqualFunc) stats_cluster_key_equal,
                                   NULL,
                                   (GDestroyNotify) _unregister_single_cluster_locked);
}

static void
_deinit_tls_clusters_map_thread_init_hook(gpointer user_data)
{
  g_hash_table_destroy(clusters);
}

static void
_init_tls_clusters_map_apphook(gint type, gpointer user_data)
{
  _init_tls_clusters_map_thread_init_hook(user_data);
}

static void
_deinit_tls_clusters_map_apphook(gint type, gpointer user_data)
{
  _deinit_tls_clusters_map_thread_init_hook(user_data);
}

static void
_register_global_initializers(void)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      register_application_thread_init_hook(_init_tls_clusters_map_thread_init_hook, NULL);
      register_application_thread_deinit_hook(_deinit_tls_clusters_map_thread_init_hook, NULL);
      register_application_hook(AH_STARTUP, _init_tls_clusters_map_apphook, NULL, AHM_RUN_ONCE);
      register_application_hook(AH_SHUTDOWN, _deinit_tls_clusters_map_apphook, NULL, AHM_RUN_ONCE);
      initialized = TRUE;
    }
}

static gboolean
_init(LogPipe *s)
{
  MetricsProbe *self = (MetricsProbe *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  log_template_options_init(&self->template_options, cfg);

  if (!self->key && !self->label_templates)
    {
      metrics_probe_set_key(&self->super, "classified_events_total");

      _add_default_label_template(self, "app", "${APP}");
      _add_default_label_template(self, "host", "${HOST}");
      _add_default_label_template(self, "program", "${PROGRAM}");
      _add_default_label_template(self, "source", "${SOURCE}");
    }

  if (!self->key)
    {
      msg_error("metrics-probe: Setting key() is mandatory, when not using the default values",
                log_pipe_location_tag(s));
      return FALSE;
    }

  self->num_of_static_labels = g_list_length(self->label_templates);
  self->label_templates = g_list_sort(self->label_templates, (GCompareFunc) label_template_compare);
  self->label_buffers = g_array_set_size(self->label_buffers, self->num_of_static_labels);

  _register_global_initializers();

  return log_parser_init_method(s);
}

static LogPipe *
_clone(LogPipe *s)
{
  MetricsProbe *self = (MetricsProbe *) s;
  MetricsProbe *cloned = (MetricsProbe *) metrics_probe_new(s->cfg);

  log_parser_clone_settings(&self->super, &cloned->super);
  metrics_probe_set_key(&cloned->super, self->key);

  for (GList *elem = g_list_first(self->label_templates); elem; elem = elem->next)
    {
      LabelTemplate *label_template = (LabelTemplate *) elem->data;
      cloned->label_templates = g_list_append(cloned->label_templates, label_template_clone(label_template));
    }

  cloned->num_of_static_labels = self->num_of_static_labels;
  cloned->label_buffers = g_array_set_size(cloned->label_buffers, cloned->num_of_static_labels);

  metrics_probe_set_increment_template(&cloned->super, self->increment_template);
  metrics_probe_set_level(&cloned->super, self->level);
  log_template_options_clone(&self->template_options, &cloned->template_options);

  return &cloned->super.super;
}

static void
_free(LogPipe *s)
{
  MetricsProbe *self = (MetricsProbe *) s;

  g_free(self->key);
  g_list_free_full(self->label_templates, (GDestroyNotify) label_template_free);
  log_template_unref(self->increment_template);
  log_template_options_destroy(&self->template_options);
  g_array_free(self->label_buffers, TRUE);

  log_parser_free_method(s);
}

LogParser *
metrics_probe_new(GlobalConfig *cfg)
{
  MetricsProbe *self = g_new0(MetricsProbe, 1);

  log_parser_init_instance(&self->super, cfg);
  self->super.super.init = _init;
  self->super.super.free_fn = _free;
  self->super.super.clone = _clone;
  self->super.process = _process;

  self->label_buffers = g_array_sized_new(FALSE, FALSE, sizeof(StatsClusterLabel), 4);
  log_template_options_defaults(&self->template_options);

  return &self->super;
}
