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

#include "rewrite-subst.h"

/* LogRewriteSubst
 *
 * This class implements the "subst" expression in a rewrite rule.
 */
typedef struct _LogRewriteSubst LogRewriteSubst;

struct _LogRewriteSubst
{
  LogRewrite super;
  LogMatcherOptions matcher_options;
  LogMatcher *matcher;
  LogTemplate *replacement;
};

LogMatcherOptions *
log_rewrite_subst_get_matcher_options(LogRewrite *s)
{
  LogRewriteSubst *self = (LogRewriteSubst *) s;

  return &self->matcher_options;
}

void
log_rewrite_subst_process(LogRewrite *s, LogMessage **pmsg, const LogPathOptions *path_options)
{
  LogRewriteSubst *self = (LogRewriteSubst *) s;
  LogMessage *msg;
  NVTable *nvtable;
  const gchar *value;
  gchar *new_value;
  gssize length;
  gssize new_length = -1;

  msg = log_msg_make_writable(pmsg, path_options);
  nvtable = nv_table_ref(msg->payload);
  value = log_msg_get_value(msg, self->super.value_handle, &length);
  new_value = log_matcher_replace(self->matcher, msg, self->super.value_handle, value, length, self->replacement,
                                  &new_length);

  if (new_value)
    {
      msg_trace("Performing subst() rewrite",
                evt_tag_str("rule", s->name),
                evt_tag_str("value", log_msg_get_value_name(s->value_handle, NULL)),
                evt_tag_printf("input", "%.*s", (gint) length, value),
                evt_tag_str("type", self->matcher_options.type),
                evt_tag_str("pattern", self->matcher->pattern),
                evt_tag_str("replacement", self->replacement->template),
                log_pipe_location_tag(&s->super));
      log_msg_set_value(msg, self->super.value_handle, new_value, new_length);
    }
  else
    {
      msg_trace("Performing subst() rewrite failed, pattern did not match",
                evt_tag_str("rule", s->name),
                evt_tag_str("value", log_msg_get_value_name(s->value_handle, NULL)),
                evt_tag_printf("input", "%.*s", (gint) length, value),
                evt_tag_str("type", self->matcher_options.type),
                evt_tag_str("pattern", self->matcher->pattern),
                evt_tag_str("replacement", self->replacement->template),
                log_pipe_location_tag(&s->super));
    }
  nv_table_unref(nvtable);
  g_free(new_value);
}

gboolean
log_rewrite_subst_compile_pattern(LogRewrite *s, const gchar *regexp, GError **error)
{
  LogRewriteSubst *self = (LogRewriteSubst *) s;
  GlobalConfig *cfg = log_pipe_get_config(&s->super);

  log_matcher_options_init(&self->matcher_options, cfg);
  self->matcher = log_matcher_new(cfg, &self->matcher_options);

  if (!log_matcher_is_replace_supported(self->matcher))
    {
      g_set_error(error, LOG_MATCHER_ERROR, 0,
                  "subst() only supports matchers that allow replacement, glob is not one of these");
      return FALSE;
    }

  return log_matcher_compile(self->matcher, regexp, error);
}

static LogPipe *
log_rewrite_subst_clone(LogPipe *s)
{
  LogRewriteSubst *self = (LogRewriteSubst *) s;
  LogRewriteSubst *cloned;

  cloned = (LogRewriteSubst *) log_rewrite_subst_new(self->replacement, s->cfg);
  cloned->matcher = log_matcher_ref(self->matcher);
  cloned->super.value_handle = self->super.value_handle;

  if (self->super.condition)
    cloned->super.condition = filter_expr_ref(self->super.condition);

  return &cloned->super.super;
}

void
log_rewrite_subst_free(LogPipe *s)
{
  LogRewriteSubst *self = (LogRewriteSubst *) s;

  log_matcher_unref(self->matcher);
  log_template_unref(self->replacement);
  log_matcher_options_destroy(&self->matcher_options);
  log_rewrite_free_method(s);
}

LogRewrite *
log_rewrite_subst_new(LogTemplate *replacement, GlobalConfig *cfg)
{
  LogRewriteSubst *self = g_new0(LogRewriteSubst, 1);

  log_rewrite_init_instance(&self->super, cfg);

  self->super.super.free_fn = log_rewrite_subst_free;
  self->super.super.clone = log_rewrite_subst_clone;
  self->super.process = log_rewrite_subst_process;
  self->replacement = log_template_ref(replacement);
  log_matcher_options_defaults(&self->matcher_options);
  return &self->super;
}
