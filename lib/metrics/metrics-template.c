/*
 * Copyright (c) 2023 Attila Szakacs
 * Copyright (c) 2024 Balazs Scheidler <balazs.scheidler@axoflow.com>
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

#include "metrics-template.h"
#include "label-template.h"
#include "metrics-tls-cache.h"
#include "stats/stats-cluster-single.h"
#include "scratch-buffers.h"

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
  metrics_tls_cache_append_label(&label);

  return FALSE;
}

static void
_add_dynamic_labels(MetricsTemplate *self, LogTemplateOptions *template_options, LogMessage *msg)
{
  LogTemplateEvalOptions template_eval_options = { template_options, LTZ_SEND, 0, NULL, LM_VT_STRING };
  value_pairs_foreach(self->vp, _add_dynamic_labels_vp_helper, msg, &template_eval_options, NULL);
}

gboolean
metrics_template_is_enabled(MetricsTemplate *self)
{
  return stats_check_level(self->level);
}

void
metrics_template_build_sck(MetricsTemplate *self,
                           LogTemplateOptions *template_options,
                           LogMessage *msg, StatsClusterKey *key)
{
  metrics_tls_cache_reset_labels();

  for (GList *elem = g_list_first(self->label_templates); elem; elem = elem->next)
    {
      LabelTemplate *label_template = (LabelTemplate *) elem->data;
      GString *value_buffer = scratch_buffers_alloc();

      label_template_format(label_template, template_options, msg, value_buffer,
                            metrics_tls_cache_get_next_label());
    }

  if (self->vp)
    _add_dynamic_labels(self, template_options, msg);

  stats_cluster_single_key_set(key, self->key, metrics_tls_cache_get_labels(), metrics_tls_cache_get_labels_len());
}

StatsCounterItem *
metrics_template_get_stats_counter(MetricsTemplate *self,
                                   LogTemplateOptions *template_options,
                                   LogMessage *msg)
{
  StatsClusterKey key;
  ScratchBuffersMarker marker;

  scratch_buffers_mark(&marker);
  metrics_template_build_sck(self, template_options, msg, &key);

  StatsCounterItem *counter = metrics_tls_cache_get_counter(&key, self->level);

  scratch_buffers_reclaim_marked(marker);
  return counter;
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

void
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
