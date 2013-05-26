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

#include "rewrite-subst.h"

/* LogRewriteSubst
 *
 * This class implements the "subst" expression in a rewrite rule.
 */
typedef struct _LogRewriteSubst LogRewriteSubst;

struct _LogRewriteSubst
{
  LogRewrite super;
  LogMatcher *matcher;
  LogTemplate *replacement;
};

void
log_rewrite_subst_process(LogRewrite *s, LogMessage **pmsg, const LogPathOptions *path_options)
{
  LogRewriteSubst *self = (LogRewriteSubst *) s;
  const gchar *value;
  gchar *new_value;
  gssize length;
  gssize new_length = -1;

  value = log_msg_get_value(*pmsg, self->super.value_handle, &length);

  new_value = log_matcher_replace(self->matcher, *pmsg, self->super.value_handle, value, length, self->replacement, &new_length);
  if (new_value)
    {
      log_msg_make_writable(pmsg, path_options);
      log_msg_set_value(*pmsg, self->super.value_handle, new_value, new_length);
    }
  g_free(new_value);
}

void
log_rewrite_subst_set_matcher(LogRewrite *s, LogMatcher *matcher)
{
  LogRewriteSubst *self = (LogRewriteSubst*) s;
  gint flags = 0;

  if(self->matcher)
    {
      flags = self->matcher->flags;
      log_matcher_unref(self->matcher);
    }
  self->matcher = matcher;

  log_rewrite_subst_set_flags(s, flags);
}

gboolean
log_rewrite_subst_set_regexp(LogRewrite *s, const gchar *regexp)
{
  LogRewriteSubst *self = (LogRewriteSubst*) s;

  if (!self->matcher)
    self->matcher = log_matcher_posix_re_new();

  return log_matcher_compile(self->matcher, regexp);
}

void
log_rewrite_subst_set_flags(LogRewrite *s, gint flags)
{
  LogRewriteSubst *self = (LogRewriteSubst*)s;

  if (!self->matcher)
    self->matcher = log_matcher_posix_re_new();

  log_matcher_set_flags(self->matcher, flags);
}

static LogPipe *
log_rewrite_subst_clone(LogPipe *s)
{
  LogRewriteSubst *self = (LogRewriteSubst *) s;
  LogRewriteSubst *cloned;

  cloned = (LogRewriteSubst *) log_rewrite_subst_new(self->replacement->template);
  cloned->matcher = log_matcher_ref(self->matcher);
  cloned->super.value_handle = self->super.value_handle;
  cloned->super.condition = self->super.condition;
  return &cloned->super.super;
}


void
log_rewrite_subst_free(LogPipe *s)
{
  LogRewriteSubst *self = (LogRewriteSubst *) s;

  log_matcher_unref(self->matcher);
  log_template_unref(self->replacement);
  log_rewrite_free_method(s);
}

LogRewrite *
log_rewrite_subst_new(const gchar *replacement)
{
  LogRewriteSubst *self = g_new0(LogRewriteSubst, 1);

  log_rewrite_init(&self->super);

  self->super.super.free_fn = log_rewrite_subst_free;
  self->super.super.clone = log_rewrite_subst_clone;
  self->super.process = log_rewrite_subst_process;

  self->replacement = log_template_new(configuration, NULL);
  log_template_compile(self->replacement, replacement, NULL);
  return &self->super;
}
