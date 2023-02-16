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

#define NUM_OF_LABELS_MAX (128)

typedef struct _MetricsProbe
{
  LogParser super;

  gchar *key;
  GList *label_templates;
  guint8 num_of_labels;
} MetricsProbe;

void
metrics_probe_set_key(LogParser *s, const gchar *key)
{
  MetricsProbe *self = (MetricsProbe *) s;

  g_free(self->key);
  self->key = g_strdup(key);
}

gboolean
metrics_probe_add_label_template(LogParser *s, const gchar *label, LogTemplate *value_template)
{
  MetricsProbe *self = (MetricsProbe *) s;

  if (self->num_of_labels >= NUM_OF_LABELS_MAX)
    return FALSE;

  self->label_templates = g_list_append(self->label_templates, label_template_new(label, value_template));
  self->num_of_labels++;

  return TRUE;
}

static gboolean
_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input, gsize input_len)
{
  MetricsProbe *self = (MetricsProbe *) s;

  msg_trace("metrics-probe message processing started",
            evt_tag_str("key", self->key),
            evt_tag_msg_reference(*pmsg));

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

static gboolean
_init(LogPipe *s)
{
  MetricsProbe *self = (MetricsProbe *) s;

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

  self->label_templates = g_list_sort(self->label_templates, (GCompareFunc) label_template_compare);

  return log_parser_init_method(s);
}

static LogPipe *
_clone(LogPipe *s)
{
  MetricsProbe *self = (MetricsProbe *) s;
  MetricsProbe *cloned = (MetricsProbe *) metrics_probe_new(s->cfg);

  log_parser_set_template(&cloned->super, log_template_ref(self->super.template));
  metrics_probe_set_key(&cloned->super, self->key);

  for (GList *elem = g_list_first(self->label_templates); elem; elem = elem->next)
    {
      LabelTemplate *label_template = (LabelTemplate *) elem->data;
      cloned->label_templates = g_list_append(cloned->label_templates, label_template_clone(label_template));
    }

  return &cloned->super.super;
}

static void
_free(LogPipe *s)
{
  MetricsProbe *self = (MetricsProbe *) s;

  g_free(self->key);
  g_list_free_full(self->label_templates, (GDestroyNotify) label_template_free);

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

  return &self->super;
}
