/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#include "filter/filter-expr.h"
#include "filter/filter-expr-grammar.h"
#include "filter/filter-pipe.h"
#include "syslog-names.h"
#include "messages.h"
#include "cfg.h"
#include "gsocket.h"
#include "misc.h"
#include "tags.h"
#include "cfg-tree.h"

#include <regex.h>
#include <string.h>
#include <stdlib.h>

/****************************************************************
 * Filter expression nodes
 ****************************************************************/

void
filter_expr_node_init(FilterExprNode *self)
{
  self->ref_cnt = 1;
}

gboolean
filter_expr_eval_with_context(FilterExprNode *self, LogMessage **msg, gint num_msg)
{
  gboolean res;

  res = self->eval(self, msg, num_msg);
  msg_debug("Filter node evaluation result",
            evt_tag_str("result", res ? "match" : "not-match"),
            evt_tag_str("type", self->type),
            NULL);
  return res;
}

gboolean
filter_expr_eval(FilterExprNode *self, LogMessage *msg)
{
  return filter_expr_eval_with_context(self, &msg, 1);
}

FilterExprNode *
filter_expr_ref(FilterExprNode *self)
{
  self->ref_cnt++;
  return self;
}

void
filter_expr_unref(FilterExprNode *self)
{
  if (--self->ref_cnt == 0)
    {
      if (self->free_fn)
        self->free_fn(self);
      g_free(self);
    }
}



typedef struct _FilterPri
{
  FilterExprNode super;
  guint32 valid;
} FilterPri;

static gboolean
filter_facility_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg)
{
  FilterPri *self = (FilterPri *) s;
  LogMessage *msg = msgs[0];
  guint32 fac_num = (msg->pri & LOG_FACMASK) >> 3;

  if (G_UNLIKELY(self->valid & 0x80000000))
    {
      /* exact number specified */
      return ((self->valid & ~0x80000000) == fac_num) ^ s->comp;
    }
  else
    {
      return !!(self->valid & (1 << fac_num)) ^ self->super.comp;
    }
  return self->super.comp;
}

FilterExprNode *
filter_facility_new(guint32 facilities)
{
  FilterPri *self = g_new0(FilterPri, 1);

  filter_expr_node_init(&self->super);
  self->super.eval = filter_facility_eval;
  self->valid = facilities;
  self->super.type = "facility";
  return &self->super;
}

static gboolean
filter_level_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg)
{
  FilterPri *self = (FilterPri *) s;
  LogMessage *msg = msgs[0];
  guint32 pri = msg->pri & LOG_PRIMASK;


  return !!((1 << pri) & self->valid) ^ self->super.comp;
}

FilterExprNode *
filter_level_new(guint32 levels)
{
  FilterPri *self = g_new0(FilterPri, 1);

  filter_expr_node_init(&self->super);
  self->super.eval = filter_level_eval;
  self->valid = levels;
  self->super.type = "level";
  return &self->super;
}

static gboolean
filter_re_eval_string(FilterExprNode *s, LogMessage *msg, gint value_handle, const gchar *str, gssize str_len)
{
  FilterRE *self = (FilterRE *) s;

  if (str_len < 0)
    str_len = strlen(str);

  return log_matcher_match(self->matcher, msg, value_handle, str, str_len) ^ self->super.comp;
}

static gboolean
filter_re_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg)
{
  FilterRE *self = (FilterRE *) s;
  const gchar *value;
  LogMessage *msg = msgs[0];
  gssize len = 0;

  value = log_msg_get_value(msg, self->value_handle, &len);

  APPEND_ZERO(value, value, len);
  return filter_re_eval_string(s, msg, self->value_handle, value, len);
}


static void
filter_re_free(FilterExprNode *s)
{
  FilterRE *self = (FilterRE *) s;

  log_matcher_unref(self->matcher);
}

void
filter_re_set_matcher(FilterRE *self, LogMatcher *matcher)
{
  gint flags = 0;
  if(self->matcher)
    {
      /* save the flags to use them in the new matcher */
      flags = self->matcher->flags;
      log_matcher_unref(self->matcher);
    }
   self->matcher = matcher;

   filter_re_set_flags(self, flags);
}

void
filter_re_set_flags(FilterRE *self, gint flags)
{
  /* if there is only a flags() param, we must crete the default matcher*/
  if(!self->matcher)
    self->matcher = log_matcher_posix_re_new();
  if (flags & LMF_STORE_MATCHES)
    self->super.modify = TRUE;
  log_matcher_set_flags(self->matcher, flags | LMF_MATCH_ONLY);
}

gboolean
filter_re_set_regexp(FilterRE *self, gchar *re)
{
  if(!self->matcher)
    self->matcher = log_matcher_posix_re_new();

  return log_matcher_compile(self->matcher, re);
}

FilterExprNode *
filter_re_new(NVHandle value_handle)
{
  FilterRE *self = g_new0(FilterRE, 1);

  filter_expr_node_init(&self->super);
  self->value_handle = value_handle;
  self->super.eval = filter_re_eval;
  self->super.free_fn = filter_re_free;
  return &self->super;
}

static gboolean
filter_match_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg)
{
  FilterRE *self = (FilterRE *) s;
  gchar *str;
  gboolean res;
  LogMessage *msg = msgs[0];

  if (G_UNLIKELY(!self->value_handle))
    {
      const gchar *pid;
      gssize pid_len;

      pid = log_msg_get_value(msg, LM_V_PID, &pid_len);

      /* compatibility mode */
      str = g_strdup_printf("%s%s%s%s: %s",
                            log_msg_get_value(msg, LM_V_PROGRAM, NULL),
                            pid_len > 0 ? "[" : "",
                            pid,
                            pid_len > 0 ? "]" : "",
                            log_msg_get_value(msg, LM_V_MESSAGE, NULL));
      res = filter_re_eval_string(s, msg, LM_V_NONE, str, -1);
      g_free(str);
    }
  else
    {
      res = filter_re_eval(s, msgs, num_msg);
    }
  return res;
}

FilterExprNode *
filter_match_new()
{
  FilterRE *self = g_new0(FilterRE, 1);

  filter_expr_node_init(&self->super);
  self->super.free_fn = filter_re_free;
  self->super.eval = filter_match_eval;
  return &self->super;
}
