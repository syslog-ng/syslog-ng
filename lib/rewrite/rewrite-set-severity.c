/*
 * Copyright (c) 2019 Balabit
 * Copyright (c) 2019 Kokan <kokaipeter@gmail.com>
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

#include "rewrite-set-severity.h"
#include "template/templates.h"
#include "syslog-names.h"
#include "scratch-buffers.h"

#include <stdlib.h>

typedef struct _LogRewriteSetSeverity LogRewriteSetSeverity;

struct _LogRewriteSetSeverity
{
  LogRewrite super;
  LogTemplate *severity;
};

static gint
_convert_severity_as_number(GString *severity_text)
{
  char *endptr;
  long int severity = strtol(severity_text->str, &endptr, 10);
  if (0 == endptr)
    return -1;

  if (endptr[0] != '\0')
    return -1;

  if (severity > 7)
    return -1;

  return severity;
}

static gint
_convert_severity_as_text(GString *severity_text)
{
  return syslog_name_lookup_level_by_name(severity_text->str);
}

static gint
_convert_severity(GString *severity_text)
{
  gint severity = _convert_severity_as_number(severity_text);
  if (severity > 0)
    return severity;

  severity = _convert_severity_as_text(severity_text);
  if (severity > 0)
    return severity;

  return -1;
}

static void
_set_msg_severity(LogMessage *msg, const guint16 severity)
{
  msg->pri = (msg->pri & ~LOG_PRIMASK) | severity;
}

static void
log_rewrite_set_severity_process(LogRewrite *s, LogMessage **pmsg, const LogPathOptions *path_options)
{
  ScratchBuffersMarker marker;
  GString *result = scratch_buffers_alloc_and_mark(&marker);
  LogRewriteSetSeverity *self = (LogRewriteSetSeverity *) s;

  log_msg_make_writable(pmsg, path_options);

  log_template_format(self->severity, *pmsg, NULL, LTZ_LOCAL, 0, NULL, result);

  const gint severity = _convert_severity(result);
  if (severity < 0)
    {
      msg_warning("Warning: invalid severity to set", evt_tag_str("severity", result->str), log_pipe_location_tag(&s->super));
      goto error;
    }

  _set_msg_severity(*pmsg, _convert_severity(result));

error:
  scratch_buffers_reclaim_marked(marker);
}

static LogPipe *
log_rewrite_set_severity_clone(LogPipe *s)
{
  LogRewriteSetSeverity *self = (LogRewriteSetSeverity *) s;
  LogRewriteSetSeverity *cloned = (LogRewriteSetSeverity *)log_rewrite_set_severity_new(log_template_ref(self->severity),
                                  s->cfg);

  if (self->super.condition)
    cloned->super.condition = filter_expr_ref(self->super.condition);

  return &cloned->super.super;
}

void
log_rewrite_set_severity_free(LogPipe *s)
{
  LogRewriteSetSeverity *self = (LogRewriteSetSeverity *) s;
  log_template_unref(self->severity);
  log_rewrite_free_method(s);
}

LogRewrite *
log_rewrite_set_severity_new(LogTemplate *severity, GlobalConfig *cfg)
{
  LogRewriteSetSeverity *self = g_new0(LogRewriteSetSeverity, 1);

  log_rewrite_init_instance(&self->super, cfg);
  self->super.super.free_fn = log_rewrite_set_severity_free;
  self->super.super.clone = log_rewrite_set_severity_clone;
  self->super.process = log_rewrite_set_severity_process;
  self->severity = severity;
  return &self->super;
}
