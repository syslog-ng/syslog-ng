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

#include "logmpx.h"


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
log_multiplexer_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options, gpointer user_data)
{
  LogMultiplexer *self = (LogMultiplexer *) s;
  gint i;
  LogPathOptions local_options = *path_options;
  gboolean matched;
  gboolean delivered = FALSE;
  gboolean last_delivery;
  gint fallback;
  
  local_options.matched = &matched;
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
          
          /* NOTE: this variable indicates that the upcoming message
           * delivery is the last one, thus we don't need to retain an an
           * unmodified copy to be sent to further paths.  The current
           * delivery may modify the message at will.
           */
           
          last_delivery = (self->super.pipe_next == NULL) && 
                          (i == self->next_hops->len - 1) && 
                          (!self->fallback_exists || delivered || fallback == 1);
          
          if (!last_delivery)
            log_msg_write_protect(msg);
          log_pipe_queue(next_hop, log_msg_ref(msg), &local_options);
          if (!last_delivery)
            log_msg_write_unprotect(msg);
          
          if (matched)
            {
              delivered = TRUE; 
              if (G_UNLIKELY(next_hop->flags & PIF_BRANCH_FINAL))
                break;
            }
        }
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

LogMultiplexer *
log_multiplexer_new(guint32 flags)
{
  LogMultiplexer *self = g_new0(LogMultiplexer, 1);
  
  log_pipe_init_instance(&self->super);
  self->super.init = log_multiplexer_init;
  self->super.deinit = log_multiplexer_deinit;
  self->super.queue = log_multiplexer_queue;
  self->super.free_fn = log_multiplexer_free;
  self->next_hops = g_ptr_array_new();
  self->super.flags = flags;
  return self;
}
