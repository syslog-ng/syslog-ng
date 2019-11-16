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

static void
log_multiplexer_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogMultiplexer *self = (LogMultiplexer *) s;
  gint i;
  LogPathOptions local_options = *path_options;
  gboolean matched;
  gboolean delivered = FALSE;
  gint fallback;

  local_options.matched = &matched;
  if (self->next_hops->len > 1)
    {
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
          log_msg_add_ack(msg, &local_options);
          log_pipe_queue(next_hop, log_msg_ref(msg), &local_options);

          if (matched)
            {
              delivered = TRUE;
              if (G_UNLIKELY(next_hop->flags & PIF_BRANCH_FINAL))
                break;
            }
        }
    }
  if (self->next_hops->len > 1)
    {
      log_msg_write_unprotect(msg);
    }

  /* NOTE: non of our multiplexed destinations delivered this message, let's
   * propagate this result.  But only if we don't have a "next".  If we do,
   * that would be responsible for doing the same, for instance if it is a
   * filter.
   *
   * In case where this matters most (e.g.  the multiplexer attached to the
   * source LogPipe), "next" will always be NULL.  I am not sure if there's
   * ever a case, where a LogMpx has both "next" set, and have branches as
   * well.
   *
   * If there's such a case, then from a conceptual point of view, this Mpx
   * instance should transfer the responsibility for setting "matched" to
   * the next pipeline element, which is what we do here.
   */

  if (!s->pipe_next && !delivered && path_options->matched)
    *path_options->matched = FALSE;

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
  return self;
}
