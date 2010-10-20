/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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
log_pipe_init_instance(LogPipe *self)
{
  g_atomic_counter_set(&self->ref_cnt, 1);
  self->pipe_next = NULL;
  self->queue = log_pipe_forward_msg;
  self->free_fn = log_pipe_free_method;
/*  self->notify = log_pipe_forward_notify; */
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
log_pipe_forward_msg(LogPipe *self, LogMessage *msg, const LogPathOptions *path_options)
{
  if (self->pipe_next)
    {
      log_pipe_queue(self->pipe_next, msg, path_options);
    }
  else
    {
      log_msg_drop(msg, path_options);
    }
}

void
log_pipe_forward_notify(LogPipe *self, LogPipe *sender, gint notify_code, gpointer user_data)
{
  log_pipe_notify(self->pipe_next, self, notify_code, user_data);
}
