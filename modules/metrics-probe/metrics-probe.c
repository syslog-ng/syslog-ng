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
#include "metrics/metrics-template.h"
#include "metrics/metrics-tls-cache.h"
#include "scratch-buffers.h"

typedef struct _MetricsProbe
{
  LogParser super;
  LogTemplateOptions template_options;
  MetricsTemplate *metrics_template;
  LogTemplate *increment_template;
} MetricsProbe;

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

  if (!metrics_template_is_enabled(self->metrics_template))
    return TRUE;

  StatsCounterItem *counter = metrics_template_get_stats_counter(self->metrics_template, &self->template_options, *pmsg);
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

  /* FIXME: move this to earlier, e.g. module loading */
  metrics_tls_cache_global_init();

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
