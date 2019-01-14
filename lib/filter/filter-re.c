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

#include <string.h>

static gboolean
filter_re_eval_string(FilterExprNode *s, LogMessage *msg, gint value_handle, const gchar *str, gssize str_len)
{
  FilterRE *self = (FilterRE *) s;
  gboolean result;

  if (str_len < 0)
    str_len = strlen(str);
  result = log_matcher_match(self->matcher, msg, value_handle, str, str_len);
  msg_trace("match() evaluation started",
            evt_tag_str("input", str),
            evt_tag_str("pattern", self->matcher->pattern),
            evt_tag_str("value", log_msg_get_value_name(value_handle, NULL)),
            evt_tag_printf("msg", "%p", msg));
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

gboolean
filter_re_compile_pattern(FilterRE *self, GlobalConfig *cfg, const gchar *re, GError **error)
{
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

FilterRE *
filter_re_new(NVHandle value_handle)
{
  FilterRE *self = g_new0(FilterRE, 1);

  filter_re_init_instance(self, value_handle);
  return self;
}

FilterRE *
filter_source_new(void)
{
  FilterRE *self = filter_re_new(LM_V_SOURCE);

  if (!log_matcher_options_set_type(&self->matcher_options, "string"))
    {
      /* this can only happen if the plain text string matcher will cease to exist */
      g_assert_not_reached();
    }
  return self;
}

static gboolean
filter_match_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg)
{
  FilterRE *self = (FilterRE *) s;
  gchar *str;
  gboolean res;
  LogMessage *msg = msgs[num_msg - 1];

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

FilterRE *
filter_match_new(void)
{
  FilterRE *self = g_new0(FilterRE, 1);

  filter_re_init_instance(self, 0);
  self->super.eval = filter_match_eval;
  return self;
}
