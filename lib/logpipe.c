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
  
#include "logpipe.h"

void
log_pipe_init_instance(LogPipe *self, GlobalConfig *cfg)
{
  g_atomic_counter_set(&self->ref_cnt, 1);
  self->cfg = cfg;
  self->pipe_next = NULL;

  /* NOTE: queue == NULL means that this pipe simply forwards the
   * message along the pipeline, e.g. like it has called
   * log_msg_forward_msg. Since this is a common case, it is better
   * inlined (than to use an indirect call) for performance. */

  self->queue = NULL;
  self->free_fn = log_pipe_free_method;
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

void 
log_pipe_unref(LogPipe *self)
{
  g_assert(!self || g_atomic_counter_get(&self->ref_cnt));
    
  if (self && (g_atomic_counter_dec_and_test(&self->ref_cnt)))
    {
      if (self->free_fn)
        self->free_fn(self);
      g_free(self);
    }
}

void
log_pipe_forward_notify(LogPipe *self, gint notify_code, gpointer user_data)
{
  log_pipe_notify(self->pipe_next, notify_code, user_data);
}
