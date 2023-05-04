/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
 * Copyright (c) 2022 Balazs Scheidler <bazsi77@gmail.com>
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

#include "rewrite-set-tag.h"
#include "template/templates.h"
#include "scratch-buffers.h"

/* LogRewriteSetTag
 *
 * This class implements the "set-tag" expression in a rewrite rule.
 */
typedef struct _LogRewriteSetTag LogRewriteSetTag;

struct _LogRewriteSetTag
{
  LogRewrite super;
  LogTagId tag_id;
  gboolean value;
  LogTemplate *tag_template;
};

static void
_set_tag(LogRewriteSetTag *self, LogMessage *msg)
{
  if (self->tag_id != LOG_TAGS_UNDEF)
    {
      log_msg_set_tag_by_id_onoff(msg, self->tag_id, self->value);
      return;
    }

  GString *result = scratch_buffers_alloc();

  log_template_format(self->tag_template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, result);

  LogTagId tag_id = log_tags_get_by_name(result->str);
  log_msg_set_tag_by_id_onoff(msg, tag_id, self->value);
}

static void
_process(LogRewrite *s, LogMessage **pmsg, const LogPathOptions *path_options)
{
  LogRewriteSetTag *self = (LogRewriteSetTag *) s;

  log_msg_make_writable(pmsg, path_options);

  _set_tag(self, *pmsg);
}

static LogPipe *
_clone(LogPipe *s)
{
  LogRewriteSetTag *self = (LogRewriteSetTag *) s;
  LogRewriteSetTag *cloned;

  cloned = (LogRewriteSetTag *) log_rewrite_set_tag_new(self->tag_template, self->value, self->super.super.cfg);
  log_rewrite_clone_method(&cloned->super, &self->super);

  return &cloned->super.super;
}

static void
_free(LogPipe *s)
{
  LogRewriteSetTag *self = (LogRewriteSetTag *) s;

  log_template_unref(self->tag_template);
  log_rewrite_free_method(s);
}

LogRewrite *
log_rewrite_set_tag_new(LogTemplate *tag_template, gboolean value, GlobalConfig *cfg)
{
  LogRewriteSetTag *self = g_new0(LogRewriteSetTag, 1);

  log_rewrite_init_instance(&self->super, cfg);
  self->super.super.clone = _clone;
  self->super.super.free_fn = _free;
  self->super.process = _process;
  self->value = value;

  self->tag_template = log_template_ref(tag_template);

  if (log_template_is_literal_string(tag_template))
    {
      const gchar *tag_name = log_template_get_literal_value(tag_template, NULL);
      self->tag_id = log_tags_get_by_name(tag_name);
    }
  else
    {
      self->tag_id = LOG_TAGS_UNDEF;
    }

  return &self->super;
}
