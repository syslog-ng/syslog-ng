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
filter_re_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg, LogTemplateEvalOptions *options)
{
  FilterRE *self = (FilterRE *) s;
  LogMessage *msg = msgs[num_msg - 1];
  gboolean result;

  msg_trace("match() evaluation started against a name-value pair",
            evt_tag_msg_value_name("name", self->value_handle),
            evt_tag_msg_value("value", msg, self->value_handle),
            evt_tag_str("pattern", self->matcher->pattern),
            evt_tag_msg_reference(msg));
  result = log_matcher_match_value(self->matcher, msg, self->value_handle);
  return result ^ s->comp;
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
filter_re_compile_pattern(FilterExprNode *s, const gchar *re, GError **error)
{
  FilterRE *self = (FilterRE *) s;

  log_matcher_options_init(&self->matcher_options);
  self->matcher = log_matcher_new(&self->matcher_options);
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
filter_match_eval_against_program_pid_msg(FilterExprNode *s, LogMessage **msgs, gint num_msg,
                                          LogTemplateEvalOptions *options)
{
  FilterMatch *self = (FilterMatch *) s;

  const gchar *pid;
  gssize pid_len;
  gchar *str;
  gboolean result;
  LogMessage *msg = msgs[num_msg - 1];

  pid = log_msg_get_value(msg, LM_V_PID, &pid_len);

  /* compatibility mode */
  str = g_strdup_printf("%s%s%s%s: %s",
                        log_msg_get_value(msg, LM_V_PROGRAM, NULL),
                        pid_len > 0 ? "[" : "",
                        pid,
                        pid_len > 0 ? "]" : "",
                        log_msg_get_value(msg, LM_V_MESSAGE, NULL));

  msg_trace("match() evaluation started against constructed $PROGRAM[$PID]: $MESSAGE string for compatibility",
            evt_tag_printf("input", "%s", str),
            evt_tag_str("pattern", self->super.matcher->pattern),
            evt_tag_msg_reference(msg));

  result = log_matcher_match_buffer(self->super.matcher, msg, str, -1);

  g_free(str);
  return result ^ s->comp;
}

static gboolean
filter_match_eval_against_template(FilterExprNode *s, LogMessage **msgs, gint num_msg, LogTemplateEvalOptions *options)
{
  FilterMatch *self = (FilterMatch *) s;
  LogMessage *msg = msgs[num_msg - 1];

  msg_trace("match() evaluation started against template",
            evt_tag_template("input", self->template, msg, options),
            evt_tag_str("pattern", self->super.matcher->pattern),
            evt_tag_str("template", self->template->template_str),
            evt_tag_msg_reference(msg));
  gboolean result = log_matcher_match_template(self->super.matcher, msg, self->template, options);
  return result ^ s->comp;
}

static void
filter_match_determine_eval_function(FilterMatch *self)
{
  if (self->super.value_handle)
    self->super.super.eval = filter_re_eval;
  else if (self->template)
    self->super.super.eval = filter_match_eval_against_template;
  else
    self->super.super.eval = filter_match_eval_against_program_pid_msg;
}

static gboolean
filter_match_init(FilterExprNode *s, GlobalConfig *cfg)
{
  FilterMatch *self = (FilterMatch *) s;

  if (!filter_re_init(s, cfg))
    return FALSE;

  filter_match_determine_eval_function(self);

  return TRUE;
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
  self->super.super.init = filter_match_init;
  self->super.super.free_fn = filter_match_free;
  return &self->super.super;
}
