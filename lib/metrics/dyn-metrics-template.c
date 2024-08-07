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

#include "dyn-metrics-template.h"
#include "label-template.h"
#include "dyn-metrics-cache.h"
#include "stats/stats-cluster-single.h"
#include "scratch-buffers.h"

void
dyn_metrics_template_set_level(DynMetricsTemplate *self, gint level)
{
  self->level = level;
}

void
dyn_metrics_template_add_label_template(DynMetricsTemplate *self, const gchar *label, LogTemplate *value_template)
{
  self->label_templates = g_list_append(self->label_templates, label_template_new(label, value_template));
}

ValuePairs *
dyn_metrics_template_get_value_pairs(DynMetricsTemplate *self)
{
  return self->vp;
}

void
dyn_metrics_template_set_key(DynMetricsTemplate *self, const gchar *key)
{
  g_free(self->key);
  self->key = g_strdup(key);
}

static gboolean
_add_dynamic_labels_vp_helper(const gchar *name, LogMessageValueType type, const gchar *value, gsize value_len,
                              gpointer user_data)
{
  DynMetricsStore *cache = (DynMetricsStore *) user_data;

  GString *name_buffer = scratch_buffers_alloc();
  GString *value_buffer = scratch_buffers_alloc();
  g_string_assign(name_buffer, name);
  g_string_append_len(value_buffer, value, value_len);

  StatsClusterLabel *label = dyn_metrics_store_cache_label(cache);
  label->name = name_buffer->str;
  label->value = value_buffer->str;

  return FALSE;
}

static void
_add_dynamic_labels(DynMetricsTemplate *self, LogTemplateOptions *template_options, LogMessage *msg,
                    DynMetricsStore *cache)
{
  LogTemplateEvalOptions template_eval_options = { template_options, LTZ_SEND, 0, NULL, LM_VT_STRING };
  value_pairs_foreach(self->vp, _add_dynamic_labels_vp_helper, msg, &template_eval_options, cache);
}

gboolean
dyn_metrics_template_is_enabled(DynMetricsTemplate *self)
{
  return stats_check_level(self->level);
}

static void
_build_sck(DynMetricsTemplate *self, LogTemplateOptions *template_options, LogMessage *msg, DynMetricsStore *cache,
           StatsClusterKey *key)
{
  dyn_metrics_store_reset_labels_cache(cache);

  for (GList *elem = g_list_first(self->label_templates); elem; elem = elem->next)
    {
      LabelTemplate *label_template = (LabelTemplate *) elem->data;
      GString *value_buffer = scratch_buffers_alloc();

      label_template_format(label_template, template_options, msg, value_buffer,
                            dyn_metrics_store_cache_label(cache));
    }

  if (self->vp)
    _add_dynamic_labels(self, template_options, msg, cache);

  stats_cluster_single_key_set(key, self->key,
                               dyn_metrics_store_get_cached_labels(cache),
                               dyn_metrics_store_get_cached_labels_len(cache));
}

StatsCounterItem *
dyn_metrics_template_get_stats_counter(DynMetricsTemplate *self,
                                       LogTemplateOptions *template_options,
                                       LogMessage *msg)
{
  DynMetricsStore *cache = dyn_metrics_cache();

  StatsClusterKey key;
  ScratchBuffersMarker marker;

  scratch_buffers_mark(&marker);
  _build_sck(self, template_options, msg, cache, &key);

  StatsCounterItem *counter = dyn_metrics_store_retrieve_counter(cache, &key, self->level);

  scratch_buffers_reclaim_marked(marker);
  return counter;
}

gboolean
dyn_metrics_template_init(DynMetricsTemplate *self)
{
  self->label_templates = g_list_sort(self->label_templates, (GCompareFunc) label_template_compare);
  return TRUE;
}

DynMetricsTemplate *
dyn_metrics_template_new(GlobalConfig *cfg)
{
  DynMetricsTemplate *self = g_new0(DynMetricsTemplate, 1);

  self->vp = value_pairs_new(cfg);
  return self;
}

void
dyn_metrics_template_free(DynMetricsTemplate *self)
{
  g_free(self->key);
  g_list_free_full(self->label_templates, (GDestroyNotify) label_template_free);
  value_pairs_unref(self->vp);
  g_free(self);
}

DynMetricsTemplate *
dyn_metrics_template_clone(DynMetricsTemplate *self, GlobalConfig *cfg)
{
  DynMetricsTemplate *cloned = dyn_metrics_template_new(cfg);
  dyn_metrics_template_set_key(cloned, self->key);
  dyn_metrics_template_set_level(cloned, self->level);

  for (GList *elem = g_list_first(self->label_templates); elem; elem = elem->next)
    {
      LabelTemplate *label_template = (LabelTemplate *) elem->data;
      cloned->label_templates = g_list_append(cloned->label_templates, label_template_clone(label_template));
    }
  cloned->vp = value_pairs_ref(self->vp);
  return cloned;
}
