/*
 * Copyright (c) 2021 Balazs Scheidler <bazsi77@gmail.com>
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

#include "rewrite-set-pri.h"
#include "template/templates.h"
#include "syslog-names.h"
#include "scratch-buffers.h"

#include <stdlib.h>

typedef struct _LogRewriteSetPri LogRewriteSetPri;

struct _LogRewriteSetPri
{
  LogRewrite super;
  LogTemplate *pri;
};

static gint
_convert_pri(GString *pri_text)
{
  char *endptr;
  long int pri = strtol(pri_text->str, &endptr, 10);
  if (!endptr)
    return -1;

  if (endptr[0] != '\0' || pri_text->str == endptr)
    return -1;

  /* the maximum facility code is 127 in set-facility() */
  if (pri > 127*8 + 7)
    return -1;

  return pri;
}

static void
log_rewrite_set_pri_process(LogRewrite *s, LogMessage **pmsg, const LogPathOptions *path_options)
{
  LogRewriteSetPri *self = (LogRewriteSetPri *) s;
  GString *result = scratch_buffers_alloc();

  log_msg_make_writable(pmsg, path_options);

  log_template_format(self->pri, *pmsg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, result);

  const gint pri = _convert_pri(result);
  if (pri < 0)
    {
      msg_debug("Warning: invalid value passed to set-pri()",
                evt_tag_str("pri", result->str),
                log_pipe_location_tag(&s->super));
      return;
    }

  msg_trace("Setting syslog pri",
            evt_tag_int("old_pri", (*pmsg)->pri),
            evt_tag_int("new_pri", pri),
            evt_tag_printf("msg", "%p", *pmsg));
  (*pmsg)->pri = pri;
}

static LogPipe *
log_rewrite_set_pri_clone(LogPipe *s)
{
  LogRewriteSetPri *self = (LogRewriteSetPri *) s;
  LogRewriteSetPri *cloned = (LogRewriteSetPri *) log_rewrite_set_pri_new(log_template_ref(self->pri),
                             s->cfg);

  if (self->super.condition)
    cloned->super.condition = filter_expr_clone(self->super.condition);

  return &cloned->super.super;
}

void
log_rewrite_set_pri_free(LogPipe *s)
{
  LogRewriteSetPri *self = (LogRewriteSetPri *) s;
  log_template_unref(self->pri);
  log_rewrite_free_method(s);
}

LogRewrite *
log_rewrite_set_pri_new(LogTemplate *pri, GlobalConfig *cfg)
{
  LogRewriteSetPri *self = g_new0(LogRewriteSetPri, 1);

  log_rewrite_init_instance(&self->super, cfg);
  self->super.super.free_fn = log_rewrite_set_pri_free;
  self->super.super.clone = log_rewrite_set_pri_clone;
  self->super.process = log_rewrite_set_pri_process;
  self->pri = log_template_ref(pri);
  return &self->super;
}
