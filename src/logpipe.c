/*
 * Copyright (c) 2002, 2003, 2004 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "logpipe.h"

void
log_pipe_init_instance(LogPipe *self)
{
  self->ref_cnt = 1;
  self->pipe_next = NULL;
  self->queue = log_pipe_forward_msg;
/*  self->notify = log_pipe_forward_notify; */
}

LogPipe *
log_pipe_ref(LogPipe *self)
{
  g_assert(!self || self->ref_cnt > 0);
  if (self)
    self->ref_cnt++;
  return self;
}

void 
log_pipe_unref(LogPipe *self)
{
  g_assert(!self || self->ref_cnt > 0);
  if (self && (--self->ref_cnt == 0))
    {
      if (self->free_fn)
        self->free_fn(self);
      else
        g_free(self);
    }
}

void
log_pipe_forward_msg(LogPipe *self, LogMessage *msg, gint path_flags)
{
  log_pipe_queue(self->pipe_next, msg, path_flags);
}

void
log_pipe_forward_notify(LogPipe *self, LogPipe *sender, gint notify_code, gpointer user_data)
{
  log_pipe_notify(self->pipe_next, self, notify_code, user_data);
}
