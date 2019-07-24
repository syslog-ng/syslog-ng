/*
 * Copyright (c) 2002-2012 Balabit
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

#include "rewrite/rewrite-expr.h"

/* rewrite */

void
log_rewrite_set_condition(LogRewrite *self, FilterExprNode *condition)
{
  self->condition = condition;
}

static void
log_rewrite_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogRewrite *self = (LogRewrite *) s;

  msg_trace(">>>>>> rewrite rule evaluation begin",
            evt_tag_str("rule", self->name),
            log_pipe_location_tag(s),
            evt_tag_printf("msg", "%p", msg));
  if (self->condition && !filter_expr_eval_root(self->condition, &msg, path_options))
    {
      msg_trace("Rewrite condition unmatched, skipping rewrite",
                evt_tag_str("value", log_msg_get_value_name(self->value_handle, NULL)),
                evt_tag_str("rule", self->name),
                log_pipe_location_tag(s),
                evt_tag_printf("msg", "%p", msg));
    }
  else
    {
      self->process(self, &msg, path_options);
    }
  msg_trace("<<<<<< rewrite rule evaluation finished",
            evt_tag_str("rule", self->name),
            log_pipe_location_tag(s),
            evt_tag_printf("msg", "%p", msg));
  log_pipe_forward_msg(s, msg, path_options);
}

gboolean
log_rewrite_init_method(LogPipe *s)
{
  LogRewrite *self = (LogRewrite *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (self->condition && !filter_expr_init(self->condition, cfg))
    {
      return FALSE;
    }

  if (!self->name)
    self->name = cfg_tree_get_rule_name(&cfg->tree, ENC_REWRITE, s->expr_node);
  return TRUE;
}

void
log_rewrite_free_method(LogPipe *s)
{
  LogRewrite *self = (LogRewrite *) s;

  if (self->condition)
    filter_expr_unref(self->condition);
  g_free(self->name);
  log_pipe_free_method(s);
}

void
log_rewrite_init_instance(LogRewrite *self, GlobalConfig *cfg)
{
  log_pipe_init_instance(&self->super, cfg);
  /* indicate that the rewrite rule is changing the message */
  self->super.free_fn = log_rewrite_free_method;
  self->super.queue = log_rewrite_queue;
  self->super.init = log_rewrite_init_method;
  self->value_handle = LM_V_MESSAGE;
}
