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

typedef struct _MetricsProbe
{
  LogParser super;
} MetricsProbe;

static gboolean
_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input, gsize input_len)
{
  msg_trace("metrics-probe message processing started",
            evt_tag_msg_reference(*pmsg));

  return TRUE;
}

static gboolean
_init(LogPipe *s)
{
  return log_parser_init_method(s);
}

static LogPipe *
_clone(LogPipe *s)
{
  MetricsProbe *self = (MetricsProbe *) s;
  MetricsProbe *cloned = (MetricsProbe *) metrics_probe_new(s->cfg);

  log_parser_set_template(&cloned->super, log_template_ref(self->super.template));

  return &cloned->super.super;
}

static void
_free(LogPipe *s)
{
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
