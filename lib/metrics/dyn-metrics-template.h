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

#ifndef DYN_METRICS_TEMPLATE_H_INCLUDED
#define DYN_METRICS_TEMPLATE_H_INCLUDED

#include "template/templates.h"
#include "value-pairs/value-pairs.h"
#include "stats/stats-cluster.h"

/*
 * DynMetricsTemplate utilizes dyn-metrics-cache to achieve better performance
 * and to provide proper lifetime for dynamic counters.
 */

typedef struct _DynMetricsTemplate
{
  gchar *key;
  GList *label_templates;
  ValuePairs *vp;
  gint level;
} DynMetricsTemplate;

void dyn_metrics_template_set_key(DynMetricsTemplate *s, const gchar *key);
void dyn_metrics_template_add_label_template(DynMetricsTemplate *s, const gchar *label, LogTemplate *value_template);
void dyn_metrics_template_set_level(DynMetricsTemplate *s, gint level);
ValuePairs *dyn_metrics_template_get_value_pairs(DynMetricsTemplate *s);
gboolean dyn_metrics_template_is_enabled(DynMetricsTemplate *self);
StatsCounterItem *dyn_metrics_template_get_stats_counter(DynMetricsTemplate *self,
                                                         LogTemplateOptions *template_options,
                                                         LogMessage *msg);
gboolean dyn_metrics_template_init(DynMetricsTemplate *self);

DynMetricsTemplate *dyn_metrics_template_new(GlobalConfig *cfg);
void dyn_metrics_template_free(DynMetricsTemplate *self);
DynMetricsTemplate *dyn_metrics_template_clone(DynMetricsTemplate *self, GlobalConfig *cfg);

#endif
