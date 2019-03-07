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

#include "rewrite-set-level.h"
#include "template/templates.h"
#include "syslog-names.h"
#include "scratch-buffers.h"

#include <stdlib.h>

typedef struct _LogRewriteSetLevel LogRewriteSetLevel;

struct _LogRewriteSetLevel
{
  LogRewrite super;
  LogTemplate *level;
};

static gint
_convert_level_as_number(GString *level_text)
{
  char *endptr;
  long int level = strtol(level_text->str, &endptr, 10);
  if (0 == endptr)
    return -1;

  if (endptr[0] != '\0')
    return -1;

  if (level > 7)
    return -1;

  return level;
}

static gint
_convert_level_as_text(GString *level_text)
{
  return syslog_name_lookup_level_by_name(level_text->str);
}

static gint
_convert_level(GString *level_text)
{
  gint level = _convert_level_as_number(level_text);
  if (level > 0)
    return level;

  level = _convert_level_as_text(level_text);
  if (level > 0)
    return level;

  return -1;
}

static void
_set_msg_level(LogMessage *msg, const guint16 level)
{
  msg->pri = (msg->pri & ~LOG_PRIMASK) | level;
}

static void
log_rewrite_set_level_process(LogRewrite *s, LogMessage **pmsg, const LogPathOptions *path_options)
{
  ScratchBuffersMarker marker;
  GString *result = scratch_buffers_alloc_and_mark(&marker);
  LogRewriteSetLevel *self = (LogRewriteSetLevel *) s;

  log_msg_make_writable(pmsg, path_options);

  log_template_format(self->level, *pmsg, NULL, LTZ_LOCAL, 0, NULL, result);

  const gint level = _convert_level(result);
  if (level < 0)
    {
      msg_warning("Warning: invalid level to set", evt_tag_str("level", result->str), log_pipe_location_tag(&s->super));
      goto error;
    }

  _set_msg_level(*pmsg, _convert_level(result));

error:
  scratch_buffers_reclaim_marked(marker);
}

static LogPipe *
log_rewrite_set_level_clone(LogPipe *s)
{
  LogRewriteSetLevel *self = (LogRewriteSetLevel *) s;
  LogRewriteSetLevel *cloned = (LogRewriteSetLevel *)log_rewrite_set_level_new(log_template_ref(self->level), s->cfg);

  if (self->super.condition)
    cloned->super.condition = filter_expr_ref(self->super.condition);

  return &cloned->super.super;
}

void
log_rewrite_set_level_free(LogPipe *s)
{
  LogRewriteSetLevel *self = (LogRewriteSetLevel *) s;
  log_template_unref(self->level);
  log_rewrite_free_method(s);
}

LogRewrite *
log_rewrite_set_level_new(LogTemplate *level, GlobalConfig *cfg)
{
  LogRewriteSetLevel *self = g_new0(LogRewriteSetLevel, 1);

  log_rewrite_init_instance(&self->super, cfg);
  self->super.super.free_fn = log_rewrite_set_level_free;
  self->super.super.clone = log_rewrite_set_level_clone;
  self->super.process = log_rewrite_set_level_process;
  self->level = level;
  return &self->super;
}
