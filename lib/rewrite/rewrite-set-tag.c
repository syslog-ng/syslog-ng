/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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
};

static void
log_rewrite_set_tag_process(LogRewrite *s, LogMessage **pmsg, const LogPathOptions *path_options)
{
  LogRewriteSetTag *self = (LogRewriteSetTag *) s;

  log_msg_make_writable(pmsg, path_options);
  log_msg_set_tag_by_id_onoff(*pmsg, self->tag_id, self->value);
}

static LogPipe *
log_rewrite_set_tag_clone(LogPipe *s)
{
  LogRewriteSetTag *self = (LogRewriteSetTag *) s;
  LogRewriteSetTag *cloned;

  cloned = g_new0(LogRewriteSetTag, 1);
  log_rewrite_init_instance(&cloned->super, s->cfg);
  cloned->super.super.clone = log_rewrite_set_tag_clone;
  cloned->super.process = log_rewrite_set_tag_process;

  if (self->super.condition)
    cloned->super.condition = filter_expr_clone(self->super.condition);

  cloned->tag_id = self->tag_id;
  cloned->value = self->value;
  return &cloned->super.super;
}

LogRewrite *
log_rewrite_set_tag_new(const gchar *tag_name, gboolean value, GlobalConfig *cfg)
{
  LogRewriteSetTag *self = g_new0(LogRewriteSetTag, 1);

  log_rewrite_init_instance(&self->super, cfg);
  self->super.super.clone = log_rewrite_set_tag_clone;
  self->super.process = log_rewrite_set_tag_process;
  self->tag_id = log_tags_get_by_name(tag_name);
  self->value = value;
  return &self->super;
}
