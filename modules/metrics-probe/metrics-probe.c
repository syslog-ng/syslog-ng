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

TLS_BLOCK_START
{
  GHashTable *clusters;
  GArray *label_buffers;
}
TLS_BLOCK_END;

#define clusters __tls_deref(clusters)
#define label_buffers __tls_deref(label_buffers)


void
metrics_template_set_level(MetricsTemplate *self, gint level)
{
  self->level = level;
}

void
metrics_template_add_label_template(MetricsTemplate *self, const gchar *label, LogTemplate *value_template)
{
  self->label_templates = g_list_append(self->label_templates, label_template_new(label, value_template));
}

ValuePairs *
metrics_template_get_value_pairs(MetricsTemplate *self)
{
  return self->vp;
}

void
metrics_template_set_key(MetricsTemplate *self, const gchar *key)
{
  g_free(self->key);
  self->key = g_strdup(key);
}

static gboolean
_add_dynamic_labels_vp_helper(const gchar *name, LogMessageValueType type, const gchar *value, gsize value_len,
                              gpointer user_data)
{
  GString *name_buffer = scratch_buffers_alloc();
  GString *value_buffer = scratch_buffers_alloc();
  g_string_assign(name_buffer, name);
  g_string_append_len(value_buffer, value, value_len);

  StatsClusterLabel label = stats_cluster_label(name_buffer->str, value_buffer->str);
  label_buffers = g_array_append_vals(label_buffers, &label, 1);

  return FALSE;
}

static void
_add_dynamic_labels(MetricsTemplate *self, LogTemplateOptions *template_options, LogMessage *msg)
{
  LogTemplateEvalOptions template_eval_options = { template_options, LTZ_SEND, 0, NULL, LM_VT_STRING };
  value_pairs_foreach(self->vp, _add_dynamic_labels_vp_helper, msg, &template_eval_options, NULL);
}

static void
metrics_template_build_sck(MetricsTemplate *self, LogTemplateOptions *template_options, LogMessage *msg, StatsClusterKey *key)
{
  label_buffers = g_array_set_size(label_buffers, 0);

  gint label_idx = 0;
  for (GList *elem = g_list_first(self->label_templates); elem; elem = elem->next)
    {
      LabelTemplate *label_template = (LabelTemplate *) elem->data;
      GString *value_buffer = scratch_buffers_alloc();
      label_buffers = g_array_set_size(label_buffers, label_idx + 1);

      label_template_format(label_template, template_options, msg, value_buffer,
                            &g_array_index(label_buffers, StatsClusterLabel, label_idx));
      label_idx++;
    }

  if (self->vp)
    _add_dynamic_labels(self, template_options, msg);

  stats_cluster_single_key_set(key, self->key, (StatsClusterLabel *) label_buffers->data, label_buffers->len);
}

gboolean
metrics_template_init(MetricsTemplate *self)
{
  self->label_templates = g_list_sort(self->label_templates, (GCompareFunc) label_template_compare);
  return TRUE;
}

MetricsTemplate *
metrics_template_new(GlobalConfig *cfg)
{
  MetricsTemplate *self = g_new0(MetricsTemplate, 1);

  self->vp = value_pairs_new(cfg);
  return self;
}

static void
metrics_template_free(MetricsTemplate *self)
{
  g_free(self->key);
  g_list_free_full(self->label_templates, (GDestroyNotify) label_template_free);
  value_pairs_unref(self->vp);
  g_free(self);
}

MetricsTemplate *
metrics_template_clone(MetricsTemplate *self, GlobalConfig *cfg)
{
  MetricsTemplate *cloned = metrics_template_new(cfg);
  metrics_template_set_key(cloned, self->key);
  metrics_template_set_level(cloned, self->level);

  for (GList *elem = g_list_first(self->label_templates); elem; elem = elem->next)
    {
      LabelTemplate *label_template = (LabelTemplate *) elem->data;
      cloned->label_templates = g_list_append(cloned->label_templates, label_template_clone(label_template));
    }
  cloned->vp = value_pairs_ref(self->vp);
  return cloned;
}

typedef struct _MetricsProbe
{
  LogParser super;
  LogTemplateOptions template_options;
  MetricsTemplate *metrics_template;
  LogTemplate *increment_template;
} MetricsProbe;

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
metrics_probe_set_increment_template(LogParser *s, LogTemplate *increment_template)
{
  MetricsProbe *self = (MetricsProbe *) s;

  log_template_unref(self->increment_template);
  self->increment_template = log_template_ref(increment_template);
}

LogTemplateOptions *
metrics_probe_get_template_options(LogParser *s)
{
  MetricsProbe *self = (MetricsProbe *) s;

  return &self->template_options;
}

MetricsTemplate *
metrics_probe_get_metrics_template(LogParser *s)
{
  MetricsProbe *self = (MetricsProbe *) s;

  return self->metrics_template;
}

static StatsCounterItem *
_lookup_stats_counter(MetricsProbe *self, LogMessage *msg)
{
  StatsClusterKey key;
  ScratchBuffersMarker marker;

  scratch_buffers_mark(&marker);
  metrics_template_build_sck(self->metrics_template, &self->template_options, msg, &key);

  StatsCluster *cluster = g_hash_table_lookup(clusters, &key);
  if (!cluster)
    {
      cluster = _register_single_cluster_locked(&key, self->metrics_template->level);
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
            evt_tag_str("key", self->metrics_template->key),
            evt_tag_msg_reference(*pmsg));

  /* FIXME: envy */
  if (!stats_check_level(self->metrics_template->level))
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
  metrics_template_add_label_template(self->metrics_template, label, value_template);
  log_template_unref(value_template);
}

static void
_init_tls_clusters_map_thread_init_hook(gpointer user_data)
{
  g_assert(!clusters && !label_buffers);

  clusters = g_hash_table_new_full((GHashFunc) stats_cluster_key_hash,
                                   (GEqualFunc) stats_cluster_key_equal,
                                   NULL,
                                   (GDestroyNotify) _unregister_single_cluster_locked);
  label_buffers = g_array_new(FALSE, FALSE, sizeof(StatsClusterLabel));
}

static void
_deinit_tls_clusters_map_thread_init_hook(gpointer user_data)
{
  g_hash_table_destroy(clusters);
  g_array_free(label_buffers, TRUE);
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

  if (!self->metrics_template->key && !self->metrics_template->label_templates)
    {
      metrics_template_set_key(self->metrics_template, "classified_events_total");

      _add_default_label_template(self, "app", "${APP}");
      _add_default_label_template(self, "host", "${HOST}");
      _add_default_label_template(self, "program", "${PROGRAM}");
      _add_default_label_template(self, "source", "${SOURCE}");
    }

  if (!self->metrics_template->key)
    {
      msg_error("metrics-probe: Setting key() is mandatory, when not using the default values",
                log_pipe_location_tag(s));
      return FALSE;
    }

  if (!metrics_template_init(self->metrics_template))
    return FALSE;

  _register_global_initializers();

  return log_parser_init_method(s);
}


static LogPipe *
_clone(LogPipe *s)
{
  MetricsProbe *self = (MetricsProbe *) s;
  MetricsProbe *cloned = (MetricsProbe *) metrics_probe_new(s->cfg);

  log_parser_clone_settings(&self->super, &cloned->super);
  cloned->metrics_template = metrics_template_clone(self->metrics_template, s->cfg);

  metrics_probe_set_increment_template(&cloned->super, self->increment_template);
  log_template_options_clone(&self->template_options, &cloned->template_options);

  return &cloned->super.super;
}

static void
_free(LogPipe *s)
{
  MetricsProbe *self = (MetricsProbe *) s;

  log_template_unref(self->increment_template);
  log_template_options_destroy(&self->template_options);
  metrics_template_free(self->metrics_template);

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

  self->metrics_template = metrics_template_new(cfg);

  log_template_options_defaults(&self->template_options);

  return &self->super;
}
