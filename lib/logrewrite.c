/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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

#include "logrewrite.h"
#include "logmsg.h"
#include "cfg.h"
#include "templates.h"
#include "logmatcher.h"


/* rewrite */

void
log_rewrite_set_condition(LogRewrite *self, FilterExprNode *condition)
{
  self->condition = condition;
}

static void
log_rewrite_init(LogRewrite *self)
{
  self->value_handle = LM_V_MESSAGE;
}

void
log_rewrite_free(LogRewrite *self)
{
  if (self->condition)
    filter_expr_unref(self->condition);
  self->free_fn(self);
  g_free(self);
}

/* LogRewriteSet
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
log_rewrite_subst_process(LogRewrite *s, LogMessage *msg)
{
  LogRewriteSubst *self = (LogRewriteSubst *) s;
  GError *regexp_error;
  const gchar *value;
  gchar *new_value;
  regexp_error = NULL;
  gssize length;
  gssize new_length = -1;

  value = log_msg_get_value(msg, self->super.value_handle, &length);

  new_value = log_matcher_replace(self->matcher, msg, self->super.value_handle, value, length, self->replacement, &new_length);
  if (new_value)
    {
      log_msg_set_value(msg, self->super.value_handle, new_value, new_length);
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

void
log_rewrite_subst_free(LogRewrite *s)
{
  LogRewriteSubst *self = (LogRewriteSubst *) s;
  
  log_matcher_unref(self->matcher);
  log_template_unref(self->replacement);
}

LogRewrite *
log_rewrite_subst_new(const gchar *replacement)
{
  LogRewriteSubst *self = g_new0(LogRewriteSubst, 1);

  log_rewrite_init(&self->super);

  self->super.free_fn = log_rewrite_subst_free;
  self->super.process = log_rewrite_subst_process;
  
  self->replacement = log_template_new(configuration, NULL, replacement);
  return &self->super;
}

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
log_rewrite_set_process(LogRewrite *s, LogMessage *msg)
{
  LogRewriteSet *self = (LogRewriteSet *) s;
  GString *result;

  result = g_string_sized_new(64);
  log_template_format(self->value_template, msg, NULL, LTZ_LOCAL, 0, result);

  log_msg_set_value(msg, self->super.value_handle, result->str, result->len);
  g_string_free(result, TRUE);
}

static void
log_rewrite_set_free(LogRewrite *s)
{
  LogRewriteSet *self = (LogRewriteSet *) s;

  log_template_unref(self->value_template);
}

LogRewrite *
log_rewrite_set_new(const gchar *new_value)
{
  LogRewriteSet *self = g_new0(LogRewriteSet, 1);
  
  log_rewrite_init(&self->super);
  self->super.free_fn = log_rewrite_set_free;
  self->super.process = log_rewrite_set_process;

  self->value_template = log_template_new(configuration, NULL, new_value);
  
  return &self->super;
}


typedef struct _LogRewriteRule
{
  LogProcessRule super;
  GList *rewrite_list; /* LogRewrite*  */;
} LogRewriteRule;

static void 
log_rewrite_rule_call_item(gpointer item, gpointer user_data)
{
  LogRewrite *r = (LogRewrite *) item;
  LogMessage *msg = (LogMessage *) user_data;
  gssize length;
  const gchar *value;

  if (r->condition && !filter_expr_eval(r->condition, msg))
    {
      msg_debug("Rewrite condition unmatched, skipping rewrite",
                evt_tag_str("value", log_msg_get_value_name(r->value_handle, NULL)),
                NULL);
      return;
    }
  r->process(r, msg);
  if (G_UNLIKELY(debug_flag))
    {
      value = log_msg_get_value(msg, r->value_handle, &length);
      msg_debug("Rewrite expression evaluation result",
                evt_tag_str("value", log_msg_get_value_name(r->value_handle, NULL)),
                evt_tag_printf("new_value", "%.*s", (gint) length, value),
                NULL);
    }
}


static gboolean
log_rewrite_rule_process(LogProcessRule *s, LogMessage *msg)
{
  LogRewriteRule *self = (LogRewriteRule *) s;
  
  g_list_foreach(self->rewrite_list, log_rewrite_rule_call_item, msg);

  return TRUE;
}


static void
log_rewrite_rule_free(LogProcessRule *s)
{
  LogRewriteRule *self = (LogRewriteRule *) s;

  /* free each item in the list*/
  g_list_foreach(self->rewrite_list, (GFunc)log_rewrite_free,NULL);

  /* free the glist */
  g_list_free(self->rewrite_list);
  self->rewrite_list = NULL;
}

/*
 * LogRewriteRule, a configuration block encapsulating a LogRewrite instance.
 */ 
LogProcessRule *
log_rewrite_rule_new(const gchar *name, GList *rule_list)
{
  LogRewriteRule *self = g_new0(LogRewriteRule, 1);
  
  log_process_rule_init(&self->super, name);
  self->super.free_fn = log_rewrite_rule_free;
  self->super.process = log_rewrite_rule_process;
  self->rewrite_list = rule_list;
  return &self->super;
}

