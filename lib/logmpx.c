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

#include "logmpx.h"
#include "cfg-walker.h"


void
log_multiplexer_add_next_hop(LogMultiplexer *self, LogPipe *next_hop)
{
  g_ptr_array_add(self->next_hops, next_hop);
}

void
log_multiplexer_disable_delivery_propagation(LogMultiplexer *self)
{
  self->delivery_propagation = FALSE;
}

static gboolean
log_multiplexer_init(LogPipe *s)
{
  LogMultiplexer *self = (LogMultiplexer *) s;
  gint i;

  for (i = 0; i < self->next_hops->len; i++)
    {
      LogPipe *branch_head = g_ptr_array_index(self->next_hops, i);
      LogPipe *p;

      for (p = branch_head; p; p = p->pipe_next)
        {
          branch_head->flags |= (p->flags & PIF_BRANCH_PROPERTIES);
        }

      if (branch_head->flags & PIF_BRANCH_FALLBACK)
        {
          self->fallback_exists = TRUE;
        }
    }
  return TRUE;
}

static gboolean
log_multiplexer_deinit(LogPipe *self)
{
  return TRUE;
}

static gboolean
_has_multiple_arcs(LogMultiplexer *self)
{
  gint num_arcs = self->next_hops->len + (self->super.pipe_next ? 1 : 0);
  return num_arcs > 1;
}

static void
log_multiplexer_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogMultiplexer *self = (LogMultiplexer *) s;
  gint i;
  gboolean matched;
  LogPathOptions local_path_options;
  gboolean delivered = FALSE;
  gint fallback;

  log_path_options_push_junction(&local_path_options, &matched, path_options);
  if (_has_multiple_arcs(self))
    {
      /* if we are delivering to multiple branches, we need to sync the
       * filterx state with our message and also need to make everything
       * write protected so that changes in those branches don't overwrite
       * data we still need */

      filterx_eval_prepare_for_fork(path_options->filterx_context, &msg, path_options);
      log_msg_write_protect(msg);
    }
  for (fallback = 0; (fallback == 0) || (fallback == 1 && self->fallback_exists && !delivered); fallback++)
    {
      for (i = 0; i < self->next_hops->len; i++)
        {
          LogPipe *next_hop = g_ptr_array_index(self->next_hops, i);

          if (G_UNLIKELY(fallback == 0 && (next_hop->flags & PIF_BRANCH_FALLBACK) != 0))
            {
              continue;
            }
          else if (G_UNLIKELY(fallback && (next_hop->flags & PIF_BRANCH_FALLBACK) == 0))
            {
              continue;
            }

          matched = TRUE;
          log_msg_add_ack(msg, &local_path_options);
          log_pipe_queue(next_hop, log_msg_ref(msg), &local_path_options);

          if (matched)
            {
              delivered = TRUE;
              if (G_UNLIKELY(next_hop->flags & PIF_BRANCH_FINAL))
                break;
            }
        }
    }

  /* NOTE: non of our multiplexed next-hops delivered this message, let's
   * propagate this result.  But only if we don't have a "next".  If we do,
   * that would be responsible for doing the same, for instance if it is a
   * filter.
   *
   * There are three distinct cases where LogMultiplexer is used:
   *
   *   1) at the tail of a LogSource, where the multiplexer is used to
   *   dispatch messages along all log statements that reference the
   *   specific source.  In this case pipe_next is NULL, only next_hops are
   *   used to connect parallel branches
   *
   *   2) as the head element of a junction, in this case pipe_next is NULL,
   *   next_hops contain the branches of the junction.  Subsequent LogPipes
   *   in the current sequence are attached at the "join_pipe" which is
   *   connected to all parallel branches of the junction.
   *
   *   3) when we connect the logpath to destinations.  In this case
   *   next_hops contain the destinations we want to deliver to.  pipe_next
   *   is used to continue along the current logpath.
   *
   * Conceptually, the matched value we propagate to our parent logpath
   * determines if our parent considers this message matched or not matched
   * by this element.
   *
   *   - If we did match (e.g. delivered == TRUE), nothing is to be done.
   *
   *   - If we did not match (e.g.  delivered == FALSE), we may need to
   *   propagate this result to our parent by setting
   *   (*path_options->matched) to FALSE.
   *
   * In the cases of 1) and 2) we perform a filtering function and we want
   * to tell our parent that we did NOT match so it can attempt another
   * route. We need to set matched to FALSE;
   *
   * In the case of 3) we dispatched to one or more destinations and even if
   * those destinations drop our message on the floor, we are not interested.
   * "matched" will be determined by all filtering elements on the log
   * path and we are not one of them.
   *
   * We differentiate between 1, 2 and 3 based on the value of
   * self->delivery_propagation which is set during compilation.  If
   * delivery_propagation is not set, we are just here for dispatching to
   * destinations (e.g.  we need to ignore their outcome), otherwise we
   * perform a filtering function, which means we need to push our filtering
   * responsibility to the next pipe element.
   *
   */

  if (self->delivery_propagation)
    {
      if (!delivered && path_options->matched)
        *path_options->matched = FALSE;
    }
  log_pipe_forward_msg(s, msg, path_options);
}

static void
log_multiplexer_free(LogPipe *s)
{
  LogMultiplexer *self = (LogMultiplexer *) s;

  g_ptr_array_free(self->next_hops, TRUE);
  log_pipe_free_method(s);
}

static void
_append(LogPipe *to, gpointer *user_data)
{
  LogPipe *from = user_data[0];
  GList **list = user_data[1];

  Arc *arc = arc_new(from, to, ARC_TYPE_NEXT_HOP);
  *list = g_list_append(*list, arc);
}

static GList *
_arcs(LogPipe *s)
{
  LogMultiplexer *self = (LogMultiplexer *)s;
  GList *list = NULL;
  g_ptr_array_foreach(self->next_hops, (GFunc)_append, (gpointer[2])
  {
    self, &list
  });
  if (s->pipe_next)
    list = g_list_append(list, arc_new((LogPipe *)self, s->pipe_next, ARC_TYPE_PIPE_NEXT));
  return list;
};

LogMultiplexer *
log_multiplexer_new(GlobalConfig *cfg)
{
  LogMultiplexer *self = g_new0(LogMultiplexer, 1);

  log_pipe_init_instance(&self->super, cfg);
  self->super.init = log_multiplexer_init;
  self->super.deinit = log_multiplexer_deinit;
  self->super.queue = log_multiplexer_queue;
  self->super.free_fn = log_multiplexer_free;
  self->next_hops = g_ptr_array_new();
  self->super.arcs = _arcs;
  self->delivery_propagation = TRUE;
  log_pipe_add_info(&self->super, "multiplexer");
  return self;
}
