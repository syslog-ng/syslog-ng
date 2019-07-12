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

#include "filter/filter-pipe.h"
#include "stats/stats-registry.h"
#include "filter/optimizer/filter-expr-optimizer.h"
#include "filter/optimizer/filter-tree-printer.h"
#include "filter/optimizer/concatenate-or-filters.h"

/*******************************************************************
 * LogFilterPipe
 *******************************************************************/

static gboolean
log_filter_pipe_init(LogPipe *s)
{
  LogFilterPipe *self = (LogFilterPipe *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!filter_expr_init(self->expr, cfg))
    return FALSE;

  if (!self->name)
    self->name = cfg_tree_get_rule_name(&cfg->tree, ENC_FILTER, s->expr_node);

  stats_lock();
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key, SCS_FILTER, self->name, NULL );
  stats_register_counter(1, &sc_key, SC_TYPE_MATCHED, &self->matched);
  stats_register_counter(1, &sc_key, SC_TYPE_NOT_MATCHED, &self->not_matched);
  stats_unlock();

  return TRUE;
}

static void
log_filter_pipe_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogFilterPipe *self = (LogFilterPipe *) s;
  gboolean res;
  gchar *filter_result;

  msg_trace(">>>>>> filter rule evaluation begin",
            evt_tag_str("rule", self->name),
            log_pipe_location_tag(s),
            evt_tag_printf("msg", "%p", msg));

  res = filter_expr_eval_root(self->expr, &msg, path_options);

  if (res)
    {
      filter_result = "MATCH - Forwarding message to the next LogPipe";
      log_pipe_forward_msg(s, msg, path_options);
      stats_counter_inc(self->matched);
    }
  else
    {
      filter_result = "UNMATCHED - Dropping message from LogPipe";
      if (path_options->matched)
        (*path_options->matched) = FALSE;
      log_msg_drop(msg, path_options, AT_PROCESSED);
      stats_counter_inc(self->not_matched);
    }
  msg_trace("<<<<<< filter rule evaluation result",
            evt_tag_str("result", filter_result),
            evt_tag_str("rule", self->name),
            log_pipe_location_tag(s),
            evt_tag_printf("msg", "%p", msg));
}

static LogPipe *
log_filter_pipe_clone(LogPipe *s)
{
  LogFilterPipe *self = (LogFilterPipe *) s;
  LogPipe *cloned = log_filter_pipe_new(filter_expr_ref(self->expr), s->cfg);
  ((LogFilterPipe *)cloned)->name = g_strdup(self->name);
  return cloned;
}

static void
log_filter_pipe_free(LogPipe *s)
{
  LogFilterPipe *self = (LogFilterPipe *) s;

  stats_lock();
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key, SCS_FILTER, self->name, NULL );
  stats_unregister_counter(&sc_key, SC_TYPE_MATCHED, &self->matched);
  stats_unregister_counter(&sc_key, SC_TYPE_NOT_MATCHED, &self->not_matched);
  stats_unlock();

  g_free(self->name);
  filter_expr_unref(self->expr);
  log_pipe_free_method(s);
}

void
log_filter_pipe_register_optimizer(LogFilterPipe *self, FilterExprOptimizer *optimizer)
{
  g_ptr_array_add(self->optimizers, optimizer);
}

static inline void
_register_optimizers(LogFilterPipe *self)
{
  log_filter_pipe_register_optimizer(self, &filter_tree_printer);
  log_filter_pipe_register_optimizer(self, &concatenate_or_filters);
}

static inline void
_run_optimizers(LogFilterPipe *self)
{
  gint i;
  FilterExprOptimizer *optimizer;

  for (i =0; i < self->optimizers->len; i++)
    {
      optimizer = g_ptr_array_index(self->optimizers, i);

      self->expr = filter_expr_optimizer_run(self->expr, optimizer);
    }
}

LogPipe *
log_filter_pipe_new(FilterExprNode *expr, GlobalConfig *cfg)
{
  LogFilterPipe *self = g_new0(LogFilterPipe, 1);

  log_pipe_init_instance(&self->super, cfg);
  self->super.init = log_filter_pipe_init;
  self->super.queue = log_filter_pipe_queue;
  self->super.free_fn = log_filter_pipe_free;
  self->super.clone = log_filter_pipe_clone;
  self->expr = expr;

  if (cfg->optimize_filters)
    {
      self->optimizers = g_ptr_array_new();

      _register_optimizers(self);
      _run_optimizers(self);

      g_ptr_array_free(self->optimizers, TRUE);
    }

  return &self->super;
}
