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

#include "filter-re.h"
#include "str-utils.h"
#include "messages.h"
#include "scratch-buffers.h"
#include <string.h>

typedef struct _FilterRE
{
  FilterExprNode super;
  NVHandle value_handle;
  LogMatcherOptions matcher_options;
  LogMatcher *matcher;
} FilterRE;


static gboolean
filter_re_eval_string(FilterExprNode *s, LogMessage *msg, gint value_handle, const gchar *str, gssize str_len)
{
  FilterRE *self = (FilterRE *) s;
  gboolean result;

  if (str_len < 0)
    str_len = strlen(str);
  msg_trace("match() evaluation started",
            evt_tag_str("input", str),
            evt_tag_str("pattern", self->matcher->pattern),
            evt_tag_str("value", log_msg_get_value_name(value_handle, NULL)),
            evt_tag_printf("msg", "%p", msg));
  result = log_matcher_match(self->matcher, msg, value_handle, str, str_len);
  return result ^ s->comp;
}

static gboolean
filter_re_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg)
{
  FilterRE *self = (FilterRE *) s;
  NVTable *payload;
  const gchar *value;
  LogMessage *msg = msgs[num_msg - 1];
  gssize len = 0;
  gboolean rc;

  payload = nv_table_ref(msg->payload);
  value = log_msg_get_value(msg, self->value_handle, &len);
  APPEND_ZERO(value, value, len);

  rc = filter_re_eval_string(s, msg, self->value_handle, value, len);

  nv_table_unref(payload);
  return rc;
}

static void
filter_re_free(FilterExprNode *s)
{
  FilterRE *self = (FilterRE *) s;

  log_matcher_unref(self->matcher);
  log_matcher_options_destroy(&self->matcher_options);
}

static gboolean
filter_re_init(FilterExprNode *s, GlobalConfig *cfg)
{
  FilterRE *self = (FilterRE *) s;

  if (self->matcher_options.flags & LMF_STORE_MATCHES)
    self->super.modify = TRUE;

  return TRUE;
}

LogMatcherOptions *
filter_re_get_matcher_options(FilterExprNode *s)
{
  FilterRE *self = (FilterRE *) s;

  return &self->matcher_options;
}

gboolean
filter_re_compile_pattern(FilterExprNode *s, GlobalConfig *cfg, const gchar *re, GError **error)
{
  FilterRE *self = (FilterRE *) s;

  log_matcher_options_init(&self->matcher_options, cfg);
  self->matcher = log_matcher_new(cfg, &self->matcher_options);
  return log_matcher_compile(self->matcher, re, error);
}

static void
filter_re_init_instance(FilterRE *self, NVHandle value_handle)
{
  filter_expr_node_init_instance(&self->super);
  self->value_handle = value_handle;
  self->super.init = filter_re_init;
  self->super.eval = filter_re_eval;
  self->super.free_fn = filter_re_free;
  self->super.type = "regexp";
  log_matcher_options_defaults(&self->matcher_options);
  self->matcher_options.flags |= LMF_MATCH_ONLY;
}

FilterExprNode *
filter_re_new(NVHandle value_handle)
{
  FilterRE *self = g_new0(FilterRE, 1);

  filter_re_init_instance(self, value_handle);
  return &self->super;
}

FilterExprNode *
filter_source_new(void)
{
  FilterRE *self = (FilterRE *) filter_re_new(LM_V_SOURCE);

  if (!log_matcher_options_set_type(&self->matcher_options, "string"))
    {
      /* this can only happen if the plain text string matcher will cease to exist */
      g_assert_not_reached();
    }
  return &self->super;
}

typedef struct _FilterMatch
{
  FilterRE super;
  LogTemplate *template;
} FilterMatch;

gboolean
filter_match_is_usage_obsolete(FilterExprNode *s)
{
  FilterMatch *self = (FilterMatch *) s;

  return self->super.value_handle == 0 && self->template == NULL;
}

void
filter_match_set_value_handle(FilterExprNode *s, NVHandle value_handle)
{
  FilterMatch *self = (FilterMatch *) s;

  self->super.value_handle = value_handle;
}

void
filter_match_set_template_ref(FilterExprNode *s, LogTemplate *template)
{
  FilterMatch *self = (FilterMatch *) s;

  log_template_unref(self->template);
  self->template = template;
}

static gboolean
filter_match_eval_against_program_pid_msg(FilterMatch *self, LogMessage **msgs, gint num_msg)
{
  const gchar *pid;
  gssize pid_len;
  gchar *str;
  gboolean res;
  LogMessage *msg = msgs[num_msg - 1];

  pid = log_msg_get_value(msg, LM_V_PID, &pid_len);

  /* compatibility mode */
  str = g_strdup_printf("%s%s%s%s: %s",
                        log_msg_get_value(msg, LM_V_PROGRAM, NULL),
                        pid_len > 0 ? "[" : "",
                        pid,
                        pid_len > 0 ? "]" : "",
                        log_msg_get_value(msg, LM_V_MESSAGE, NULL));
  res = filter_re_eval_string(&self->super.super, msg, LM_V_NONE, str, -1);
  g_free(str);
  return res;
}

static gboolean
filter_match_eval_against_template(FilterMatch *self, LogMessage **msgs, gint num_msg)
{
  LogMessage *msg = msgs[num_msg - 1];
  GString *buffer;

  buffer = scratch_buffers_alloc();

  log_template_format(self->template, msg, NULL, LTZ_LOCAL, 0, NULL, buffer);
  return filter_re_eval_string(&self->super.super, msg, LM_V_NONE, buffer->str, buffer->len);
}

static gboolean
filter_match_eval_against_trivial_template(FilterMatch *self, LogMessage **msgs, gint num_msg)
{
  LogMessage *msg = msgs[num_msg - 1];
  NVTable *payload;
  const gchar *value;
  gssize len = 0;
  gboolean rc;

  payload = nv_table_ref(msg->payload);
  value = log_template_get_trivial_value(self->template, msg, &len);
  APPEND_ZERO(value, value, len);

  rc = filter_re_eval_string(&self->super.super, msg, LM_V_NONE, value, len);

  nv_table_unref(payload);
  return rc;
}

static gboolean
filter_match_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg)
{
  FilterMatch *self = (FilterMatch *) s;

  if (G_LIKELY(self->super.value_handle))
    return filter_re_eval(s, msgs, num_msg);
  else if (self->template && log_template_is_trivial(self->template))
    return filter_match_eval_against_trivial_template(self, msgs, num_msg);
  else if (self->template)
    return filter_match_eval_against_template(self, msgs, num_msg);
  else
    return filter_match_eval_against_program_pid_msg(self, msgs, num_msg);
}

static void
filter_match_free(FilterExprNode *s)
{
  FilterMatch *self = (FilterMatch *) s;

  log_template_unref(self->template);
  filter_re_free(&self->super.super);
}

FilterExprNode *
filter_match_new(void)
{
  FilterMatch *self = g_new0(FilterMatch, 1);

  filter_re_init_instance(&self->super, 0);
  self->super.super.eval = filter_match_eval;
  self->super.super.free_fn = filter_match_free;
  return &self->super.super;
}
