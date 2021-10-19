/*
 * Copyright (c) 2020 Balazs Scheidler <bazsi77@gmail.com>
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

#include "rewrite-set-facility.h"
#include "template/templates.h"
#include "syslog-names.h"
#include "scratch-buffers.h"
#include "parse-number.h"

#include <stdlib.h>

typedef struct _LogRewriteSetFacility LogRewriteSetFacility;

struct _LogRewriteSetFacility
{
  LogRewrite super;
  LogTemplate *facility;
};

static gint
_convert_facility_as_number(const gchar *facility_text)
{
  gint64 facility = 0;

  if (!parse_dec_number(facility_text, &facility))
    return -1;

  if (facility > 127)
    return -1;

  return FACILITY_CODE(facility);
}

static gint
_convert_facility_as_text(const gchar *facility_text)
{
  return syslog_name_lookup_facility_by_name(facility_text);
}

static gint
_convert_facility(const gchar *facility_text)
{
  gint facility = _convert_facility_as_number(facility_text);
  if (facility >= 0)
    return facility;

  facility = _convert_facility_as_text(facility_text);
  if (facility >= 0)
    return facility;

  return -1;
}

static void
_set_msg_facility(LogMessage *msg, const guint32 facility)
{
  msg->pri = (msg->pri & ~LOG_FACMASK) | facility;
}

static void
log_rewrite_set_facility_process(LogRewrite *s, LogMessage **pmsg, const LogPathOptions *path_options)
{
  ScratchBuffersMarker marker;
  GString *result = scratch_buffers_alloc_and_mark(&marker);
  LogRewriteSetFacility *self = (LogRewriteSetFacility *) s;

  log_msg_make_writable(pmsg, path_options);

  log_template_format(self->facility, *pmsg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, result);

  const gint facility = _convert_facility(result->str);
  if (facility < 0)
    {
      msg_debug("Warning: invalid value passed to set-facility()",
                evt_tag_str("facility", result->str),
                log_pipe_location_tag(&s->super));
      goto error;
    }

  msg_trace("Setting syslog facility",
            evt_tag_int("old_facility", (*pmsg)->pri & LOG_FACMASK),
            evt_tag_int("new_facility", facility),
            evt_tag_printf("msg", "%p", *pmsg));
  _set_msg_facility(*pmsg, facility);

error:
  scratch_buffers_reclaim_marked(marker);
}

static LogPipe *
log_rewrite_set_facility_clone(LogPipe *s)
{
  LogRewriteSetFacility *self = (LogRewriteSetFacility *) s;
  LogRewriteSetFacility *cloned = (LogRewriteSetFacility *)log_rewrite_set_facility_new(log_template_ref(self->facility),
                                  s->cfg);

  if (self->super.condition)
    cloned->super.condition = filter_expr_clone(self->super.condition);

  return &cloned->super.super;
}

void
log_rewrite_set_facility_free(LogPipe *s)
{
  LogRewriteSetFacility *self = (LogRewriteSetFacility *) s;
  log_template_unref(self->facility);
  log_rewrite_free_method(s);
}

LogRewrite *
log_rewrite_set_facility_new(LogTemplate *facility, GlobalConfig *cfg)
{
  LogRewriteSetFacility *self = g_new0(LogRewriteSetFacility, 1);

  log_rewrite_init_instance(&self->super, cfg);
  self->super.super.free_fn = log_rewrite_set_facility_free;
  self->super.super.clone = log_rewrite_set_facility_clone;
  self->super.process = log_rewrite_set_facility_process;
  self->facility = log_template_ref(facility);
  return &self->super;
}
