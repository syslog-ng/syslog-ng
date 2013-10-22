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

#include "filter-re.h"
#include "misc.h"

#include <string.h>

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

  filter_expr_node_init_instance(&self->super);
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

  filter_expr_node_init_instance(&self->super);
  self->super.free_fn = filter_re_free;
  self->super.eval = filter_match_eval;
  return &self->super;
}
