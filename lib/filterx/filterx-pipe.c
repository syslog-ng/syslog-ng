/*
 * Copyright (c) 2023 Balázs Scheidler <balazs.scheidler@axoflow.com>
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

#include "filterx/filterx-pipe.h"
#include "filterx/filterx-eval.h"
#include "stats/stats-registry.h"

typedef struct _LogFilterXPipe
{
  LogPipe super;
  gchar *name;
  GList *stmts;
} LogFilterXPipe;

static gboolean
log_filterx_pipe_init(LogPipe *s)
{
  LogFilterXPipe *self = (LogFilterXPipe *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!self->name)
    self->name = cfg_tree_get_rule_name(&cfg->tree, ENC_FILTER, s->expr_node);

  return TRUE;
}


static void
log_filterx_pipe_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogFilterXPipe *self = (LogFilterXPipe *) s;
  FilterXEvalContext eval_context;
  LogPathOptions local_path_options;
  gboolean res;

  path_options = log_path_options_chain(&local_path_options, path_options);
  filterx_eval_init_context(&eval_context, path_options->filterx_context);

  msg_trace(">>>>>> filterx rule evaluation begin",
            evt_tag_str("rule", self->name),
            log_pipe_location_tag(s),
            evt_tag_msg_reference(msg));

  NVTable *payload = nv_table_ref(msg->payload);
  res = filterx_eval_exec_statements(&eval_context, self->stmts, msg);

  msg_trace("<<<<<< filterx rule evaluation result",
            evt_tag_str("result", res ? "matched" : "unmatched"),
            evt_tag_str("rule", self->name),
            log_pipe_location_tag(s),
            evt_tag_int("dirty", filterx_scope_is_dirty(eval_context.scope)),
            evt_tag_msg_reference(msg));

  local_path_options.filterx_context = &eval_context;
  if (res)
    {
      log_pipe_forward_msg(s, msg, path_options);
    }
  else
    {
      if (path_options->matched)
        (*path_options->matched) = FALSE;
      log_msg_drop(msg, path_options, AT_PROCESSED);
    }

  filterx_eval_deinit_context(&eval_context);
  nv_table_unref(payload);
}

static LogPipe *
log_filterx_pipe_clone(LogPipe *s)
{
  LogFilterXPipe *self = (LogFilterXPipe *) s;
  GList *cloned_stmts = g_list_copy_deep(self->stmts, (GCopyFunc) filterx_expr_ref, NULL);

  LogPipe *cloned = log_filterx_pipe_new(cloned_stmts, s->cfg);
  ((LogFilterXPipe *)cloned)->name = g_strdup(self->name);
  return cloned;
}

static void
log_filterx_pipe_free(LogPipe *s)
{
  LogFilterXPipe *self = (LogFilterXPipe *) s;

  g_free(self->name);
  g_list_free_full(self->stmts, (GDestroyNotify) filterx_expr_unref);
  log_pipe_free_method(s);
}

LogPipe *
log_filterx_pipe_new(GList *stmts, GlobalConfig *cfg)
{
  LogFilterXPipe *self = g_new0(LogFilterXPipe, 1);

  log_pipe_init_instance(&self->super, cfg);
  self->super.flags = (self->super.flags | PIF_CONFIG_RELATED);
  self->super.init = log_filterx_pipe_init;
  self->super.queue = log_filterx_pipe_queue;
  self->super.free_fn = log_filterx_pipe_free;
  self->super.clone = log_filterx_pipe_clone;
  self->stmts = stmts;
  return &self->super;
}
