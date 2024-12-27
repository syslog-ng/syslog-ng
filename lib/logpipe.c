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

#include "logpipe.h"
#include "cfg-tree.h"
#include "cfg-walker.h"

gboolean (*pipe_single_step_hook)(LogPipe *pipe, LogMessage *msg, const LogPathOptions *path_options);

void
log_pipe_forward_msg(LogPipe *self, LogMessage *msg, const LogPathOptions *path_options)
{
  if (self->pipe_next)
    {
      log_pipe_queue(self->pipe_next, msg, path_options);
    }
  else
    {
      log_msg_drop(msg, path_options, AT_PROCESSED);
    }
}

/*
 * LogPipeQueue slow path that can potentially change "msg" and
 * "path_options", causing tail call optimization to be disabled.
 */
static void
log_pipe_queue_slow_path(LogPipe *self, LogMessage *msg, const LogPathOptions *path_options)
{
  LogPathOptions local_path_options;
  if ((self->flags & PIF_SYNC_FILTERX))
    filterx_eval_sync_message(path_options->filterx_context, &msg, path_options);

  if (G_UNLIKELY(self->flags & (PIF_HARD_FLOW_CONTROL | PIF_JUNCTION_END | PIF_CONDITIONAL_MIDPOINT)))
    {
      path_options = log_path_options_chain(&local_path_options, path_options);
      if (self->flags & PIF_HARD_FLOW_CONTROL)
        {
          local_path_options.flow_control_requested = 1;
          msg_trace("Requesting flow control", log_pipe_location_tag(self));
        }
      if (self->flags & PIF_JUNCTION_END)
        {
          log_path_options_pop_junction(&local_path_options);
        }
      if (self->flags & PIF_CONDITIONAL_MIDPOINT)
        {
          log_path_options_pop_conditional(&local_path_options);
        }
    }
  self->queue(self, msg, path_options);
}

static inline gboolean
_is_fastpath(LogPipe *self)
{
  if (self->flags & PIF_SYNC_FILTERX)
    return FALSE;

  if (self->flags & (PIF_HARD_FLOW_CONTROL | PIF_JUNCTION_END | PIF_CONDITIONAL_MIDPOINT))
    return FALSE;

  return TRUE;
}

void
log_pipe_queue(LogPipe *self, LogMessage *msg, const LogPathOptions *path_options)
{
  g_assert((self->flags & PIF_INITIALIZED) != 0);

  if (G_UNLIKELY((self->flags & PIF_CONFIG_RELATED) != 0 && pipe_single_step_hook))
    {
      if (!pipe_single_step_hook(self, msg, path_options))
        {
          log_msg_drop(msg, path_options, AT_PROCESSED);
          return;
        }
    }

  /* on the fastpath we can use tail call optimization, so we won't have a
   * series of log_pipe_queue() calls on the stack, it improves perf traces
   * if nothing else, but I believe it also helps locality by using a lot
   * less stack space */
  if (_is_fastpath(self))
    self->queue(self, msg, path_options);
  else
    log_pipe_queue_slow_path(self, msg, path_options);
}


EVTTAG *
log_pipe_location_tag(LogPipe *pipe)
{
  return log_expr_node_location_tag(pipe->expr_node);
}

void
log_pipe_attach_expr_node(LogPipe *self, LogExprNode *expr_node)
{
  self->expr_node = log_expr_node_ref(expr_node);
}

void
log_pipe_detach_expr_node(LogPipe *self)
{
  if (!self->expr_node)
    return;
  log_expr_node_unref(self->expr_node);
  self->expr_node = NULL;
}

static GList *
_arcs(LogPipe *self)
{
  if (self->pipe_next)
    return g_list_append(NULL, arc_new(self, self->pipe_next, ARC_TYPE_PIPE_NEXT));
  else
    return NULL;
}

void
log_pipe_clone_method(LogPipe *dst, const LogPipe *src)
{
  log_pipe_set_persist_name(dst, src->persist_name);
  log_pipe_set_options(dst, &src->options);
}

void
log_pipe_init_instance(LogPipe *self, GlobalConfig *cfg)
{
  g_atomic_counter_set(&self->ref_cnt, 1);
  self->cfg = cfg;
  self->pipe_next = NULL;
  self->persist_name = NULL;
  self->plugin_name = NULL;
  self->signal_slot_connector = signal_slot_connector_new();

  self->queue = log_pipe_forward_msg;
  self->free_fn = log_pipe_free_method;
  self->arcs = _arcs;
}

LogPipe *
log_pipe_new(GlobalConfig *cfg)
{
  LogPipe *self = g_new0(LogPipe, 1);

  log_pipe_init_instance(self, cfg);
  return self;
}

void
log_pipe_free_method(LogPipe *self)
{
  ;
}

LogPipe *
log_pipe_ref(LogPipe *self)
{
  g_assert(!self || g_atomic_counter_get(&self->ref_cnt) > 0);

  if (self)
    {
      g_atomic_counter_inc(&self->ref_cnt);
    }
  return self;
}

static void
_free(LogPipe *self)
{
  if (self->free_fn)
    self->free_fn(self);
  g_free((gpointer)self->persist_name);
  g_free(self->plugin_name);
  g_list_free_full(self->info, g_free);
  signal_slot_connector_unref(self->signal_slot_connector);
  g_free(self);
}

gboolean
log_pipe_unref(LogPipe *self)
{
  g_assert(!self || g_atomic_counter_get(&self->ref_cnt));

  if (self && (g_atomic_counter_dec_and_test(&self->ref_cnt)))
    {
      _free(self);
      return TRUE;
    }

  return FALSE;
}

void
log_pipe_forward_notify(LogPipe *self, gint notify_code, gpointer user_data)
{
  log_pipe_notify(self->pipe_next, notify_code, user_data);
}

void
log_pipe_set_persist_name(LogPipe *self, const gchar *persist_name)
{
  g_free((gpointer)self->persist_name);
  self->persist_name = g_strdup(persist_name);
}

const gchar *
log_pipe_get_persist_name(const LogPipe *self)
{
  return (self->generate_persist_name != NULL) ? self->generate_persist_name(self)
         : self->persist_name;
}

void
log_pipe_set_options(LogPipe *self, const LogPipeOptions *options)
{
  self->options = *options;
}

void
log_pipe_set_internal(LogPipe *self, gboolean internal)
{
  self->options.internal = internal;
}

gboolean
log_pipe_is_internal(const LogPipe *self)
{
  return self->options.internal;
}

void
log_pipe_add_info(LogPipe *self, const gchar *info)
{
  self->info = g_list_append(self->info, g_strdup(info));
}

#ifdef __linux__

void
__log_pipe_forward_msg(LogPipe *self, LogMessage *msg, const LogPathOptions *path_options)
__attribute__((alias("log_pipe_forward_msg")));

#endif
