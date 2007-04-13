#include "logpipe.h"

void
log_pipe_init_instance(LogPipe *self)
{
  self->ref_cnt = 1;
  self->pipe_next = NULL;
  self->queue = log_pipe_forward_msg;
/*  self->notify = log_pipe_forward_notify; */
}

static void
log_pipe_free_instance(LogPipe *self)
{
  if (self->free_fn)
    self->free_fn(self);
  else
    g_free(self);
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
      log_pipe_free_instance(self);
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
