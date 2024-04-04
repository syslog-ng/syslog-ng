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

#ifndef METRICS_TEMPLATE_H_INCLUDED
#define METRICS_TEMPLATE_H_INCLUDED

#include "template/templates.h"
#include "value-pairs/value-pairs.h"
#include "stats/stats-cluster.h"

typedef struct _MetricsTemplate
{
  gchar *key;
  GList *label_templates;
  ValuePairs *vp;
  gint level;
} MetricsTemplate;

void metrics_template_set_key(MetricsTemplate *s, const gchar *key);
void metrics_template_add_label_template(MetricsTemplate *s, const gchar *label, LogTemplate *value_template);
void metrics_template_set_level(MetricsTemplate *s, gint level);
ValuePairs *metrics_template_get_value_pairs(MetricsTemplate *s);
gboolean metrics_template_is_enabled(MetricsTemplate *self);
void metrics_template_build_sck(MetricsTemplate *self,
                                LogTemplateOptions *template_options,
                                LogMessage *msg,
                                StatsClusterKey *key);
StatsCounterItem *metrics_template_get_stats_counter(MetricsTemplate *self,
                                                     LogTemplateOptions *template_options,
                                                     LogMessage *msg);
gboolean metrics_template_init(MetricsTemplate *self);

MetricsTemplate *metrics_template_new(GlobalConfig *cfg);
void metrics_template_free(MetricsTemplate *self);
MetricsTemplate *metrics_template_clone(MetricsTemplate *self, GlobalConfig *cfg);


void metrics_template_global_init(void);

#endif
