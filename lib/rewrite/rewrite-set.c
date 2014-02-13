/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
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

#include "rewrite-set.h"

/* LogRewriteSet
 *
 * This class implements the "set" expression in a rewrite rule.
 */
typedef struct _LogRewriteSet LogRewriteSet;

struct _LogRewriteSet
{
  LogRewrite super;
  LogTemplate *value_template;
};

static void
log_rewrite_set_process(LogRewrite *s, LogMessage **pmsg, const LogPathOptions *path_options)
{
  LogRewriteSet *self = (LogRewriteSet *) s;
  GString *result;

  result = g_string_sized_new(64);
  log_template_format(self->value_template, *pmsg, NULL, LTZ_LOCAL, 0, NULL, result);

  log_msg_make_writable(pmsg, path_options);
  log_msg_set_value(*pmsg, self->super.value_handle, result->str, result->len);
  g_string_free(result, TRUE);
}

static LogPipe *
log_rewrite_set_clone(LogPipe *s)
{
  LogRewriteSet *self = (LogRewriteSet *) s;
  LogRewriteSet *cloned;

  cloned = (LogRewriteSet *) log_rewrite_set_new(log_template_ref(self->value_template), s->cfg);
  cloned->super.value_handle = self->super.value_handle;
  cloned->super.condition = self->super.condition;
  return &cloned->super.super;
}

static void
log_rewrite_set_free(LogPipe *s)
{
  LogRewriteSet *self = (LogRewriteSet *) s;

  log_template_unref(self->value_template);
  log_rewrite_free_method(s);
}

LogRewrite *
log_rewrite_set_new(LogTemplate *new_value, GlobalConfig *cfg)
{
  LogRewriteSet *self = g_new0(LogRewriteSet, 1);

  log_rewrite_init_instance(&self->super, cfg);
  self->super.super.free_fn = log_rewrite_set_free;
  self->super.super.clone = log_rewrite_set_clone;
  self->super.process = log_rewrite_set_process;
  self->value_template = log_template_ref(new_value);

  return &self->super;
}
